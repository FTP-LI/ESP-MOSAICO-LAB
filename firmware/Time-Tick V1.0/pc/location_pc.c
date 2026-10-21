// SPDX-License-Identifier: GPL-3.0-only
#include "location_pc.h"
#include <SDL.h>
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SRWLOCK lock=SRWLOCK_INIT;
static tt_location_request_t pending;
static tt_location_view_t snapshot;
static bool has_pending,running;

static char *download(const char *url)
{
    wchar_t wide[768],host[256],path[512];
    if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,url,-1,wide,768)) return NULL;
    URL_COMPONENTS parts={.dwStructSize=sizeof(parts),.lpszHostName=host,.dwHostNameLength=256,
                         .lpszUrlPath=path,.dwUrlPathLength=512};
    if (!WinHttpCrackUrl(wide,0,0,&parts) || parts.nScheme!=INTERNET_SCHEME_HTTPS) return NULL;
    HINTERNET session=WinHttpOpen(L"Time-Tick/0.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if (!session) return NULL;
    WinHttpSetTimeouts(session,5000,5000,10000,10000);
    HINTERNET connection=WinHttpConnect(session,host,parts.nPort,0);
    HINTERNET request=connection?WinHttpOpenRequest(connection,L"GET",path,NULL,WINHTTP_NO_REFERER,
                                                   WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):NULL;
    char *body=NULL; bool ok=false;
    if (!request) goto done;
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    if (!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) ||
        !WinHttpReceiveResponse(request,NULL)) goto done;
    DWORD status=0,size=sizeof(status);
    if (!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX) || status!=200) goto done;
    body=calloc(TT_LOCATION_BODY_MAX+1,1); if (!body) goto done;
    DWORD used=0,got=0;
    while (used<TT_LOCATION_BODY_MAX) {
        if (!WinHttpReadData(request,body+used,TT_LOCATION_BODY_MAX-used,&got)) goto done;
        if (!got) { ok=true; break; }
        used+=got;
    }
done:
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (!ok) { free(body); body=NULL; }
    return body;
}
static tt_location_view_t lookup(tt_location_request_t request)
{
    tt_location_view_t result={.id=request.id,.kind=request.kind,.state=TT_LOCATION_FAILED};
    char url[512];
    if (tt_location_url(&request,url,sizeof(url))) {
        char *body=download(url); tt_location_parse(body,&result); free(body);
    }
    return result;
}
static int worker(void *unused)
{
    (void)unused;
    for (;;) {
        AcquireSRWLockExclusive(&lock);
        if (!has_pending) { running=false; ReleaseSRWLockExclusive(&lock); return 0; }
        tt_location_request_t request=pending; has_pending=false;
        ReleaseSRWLockExclusive(&lock);
        tt_location_view_t result=lookup(request);
        AcquireSRWLockExclusive(&lock);
        if (snapshot.id==request.id) snapshot=result;
        ReleaseSRWLockExclusive(&lock);
    }
}
void tt_pc_location_request(const tt_location_request_t *request)
{
    AcquireSRWLockExclusive(&lock);
    pending=*request; has_pending=true;
    snapshot=(tt_location_view_t){.id=request->id,.kind=request->kind,.state=TT_LOCATION_LOADING};
    if (!running) {
        running=true;
        SDL_Thread *thread=SDL_CreateThread(worker,"location",NULL);
        if (thread) SDL_DetachThread(thread);
        else { running=has_pending=false; snapshot.state=TT_LOCATION_FAILED; }
    }
    ReleaseSRWLockExclusive(&lock);
}
void tt_pc_location_snapshot(tt_location_view_t *view)
{
    AcquireSRWLockShared(&lock); *view=snapshot; ReleaseSRWLockShared(&lock);
}
int tt_pc_location_check(void)
{
    tt_location_request_t request={.id=1,.kind=TT_LOCATION_SEARCH,.language=TT_ZH,.query="Hangzhou, CN"};
    tt_location_view_t result=lookup(request);
    if (result.state!=TT_LOCATION_READY || !result.count) {
        fprintf(stderr,"FAIL: live HTTPS location search\n"); return 1;
    }
    /* A public city query only; don't expose the computer's IP or region. */
    printf("PASS: live HTTPS city search, %u candidate(s), first=%s\n",result.count,result.places[0].name);
    return 0;
}
