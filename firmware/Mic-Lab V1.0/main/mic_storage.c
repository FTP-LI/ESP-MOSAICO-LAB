// SPDX-License-Identifier: GPL-3.0-only
#include "mic_storage.h"
#include "app_storage.h"
#include <errno.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
static QueueHandle_t jobs,results;
static int16_t *buffer;static size_t capacity;
static void worker(void *unused){
    (void)unused;size_t count;
    while(xQueueReceive(jobs,&count,portMAX_DELAY)==pdTRUE){
        mic_storage_result_t result={.loading=count==0,.result=CLIP_IO_ERROR};
        if(app_storage_acquire()==ESP_OK){
            result.mounted=true;
            if(mkdir("/nand/mic-lab",0755)==0||errno==EEXIST){
                if(count==0)result.result=clip_file_load("/nand/mic-lab",buffer,capacity,&result.samples);
                else {result.result=clip_file_save("/nand/mic-lab",buffer,count);result.samples=count;}
            }
            app_storage_release();
        }
        xQueueSend(results,&result,portMAX_DELAY);
    }
}
esp_err_t mic_storage_start(int16_t *clip,size_t cap){
    buffer=clip;capacity=cap;jobs=xQueueCreate(1,sizeof(size_t));results=xQueueCreate(1,sizeof(mic_storage_result_t));
    if(!jobs||!results)return ESP_ERR_NO_MEM;
    if(xTaskCreate(worker,"mic_store",6144,NULL,3,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
    size_t initial=0;xQueueSend(jobs,&initial,0);return ESP_OK;
}
bool mic_storage_save(size_t count){return count>0&&jobs&&xQueueSend(jobs,&count,0)==pdTRUE;}
bool mic_storage_poll(mic_storage_result_t *r){return results&&xQueueReceive(results,r,0)==pdTRUE;}
