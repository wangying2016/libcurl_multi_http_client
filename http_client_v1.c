#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>

// request
char *urls[] = {
  "https://www.microsoft.com",
  "https://opensource.org",
  "https://www.google.com",
  "https://www.yahoo.com",
  "https://www.ibm.com",
  "https://www.mysql.com",
  "https://www.oracle.com",
  "https://www.ripe.net",
  "https://www.iana.org",
  "https://www.amazon.com",
  "https://www.netcraft.com",
  "https://www.heise.de",
  "https://www.chip.de",
  "https://www.ca.com",
  "https://www.cnet.com",
  "https://www.mozilla.org",
  "https://www.cnn.com",
  "https://www.wikipedia.org",
  "https://www.dell.com",
  "https://www.hp.com",
  "https://www.cert.org",
  "https://www.mit.edu",
  "https://www.nist.gov",
  "https://www.ebay.com",
  "https://www.playstation.com",
  "https://www.uefa.com",
  "https://www.ieee.org",
  "https://www.apple.com",
  "https://www.symantec.com",
  "https://www.zdnet.com",
  "https://www.fujitsu.com/global/",
  "https://www.supermicro.com",
  "https://www.hotmail.com",
  "https://www.ietf.org",
  "https://www.bbc.co.uk",
  "https://news.google.com",
  "https://www.foxnews.com",
  "https://www.msn.com",
  "https://www.wired.com",
  "https://www.sky.com",
  "https://www.usatoday.com",
  "https://www.cbs.com",
  "https://www.nbc.com/",
  "https://slashdot.org",
  "https://www.informationweek.com",
  "https://apache.org",
  "https://www.un.org",
};


#define MAX_PARALLEL 10  
#define NUM_URLS sizeof(urls)/sizeof(char *)
#define MAX_URL_LENGTH 1024

// response struct
struct res_info {
    int index;
    // 0: initial 1: can consume 2: consume end
    int consumable;
    char url[MAX_URL_LENGTH];
};

// response mutex
pthread_mutex_t res_mutex = PTHREAD_MUTEX_INITIALIZER;

// response
struct res_info res_list[NUM_URLS];

// url index
unsigned int transfers = 0;

// request callback
static size_t write_cb(char *data, size_t n, size_t l, void *userp)
{
  /* take care of the data here, ignored in this example */ 
  (void)data;
  (void)userp;
  return n*l;
}
 
// add request
static void add_transfer(CURLM *cm, int i)
{
  CURL *eh = curl_easy_init();
  curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(eh, CURLOPT_URL, urls[i]);
  curl_easy_setopt(eh, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(eh, CURLOPT_PRIVATE, urls[i]);
  curl_multi_add_handle(cm, eh);
}

// find index
int find_index_of_urls(char *url) {
    int i;
    for (i = 0; i < NUM_URLS; ++i) {
        if (strcmp(urls[i], url) == 0) 
            return i;
    }
    return 0;
}

// add response
void add_response(struct res_info res) {
    int i = 0;
    pthread_mutex_lock(&res_mutex);
    res_list[res.index] = res;
    res_list[res.index].consumable = 1;
    // sleep(1);
    pthread_mutex_unlock(&res_mutex);
}

// consume response
void consume_response() {
    int i;
    pthread_mutex_lock(&res_mutex);
    for (i = 0; i < NUM_URLS; ++i) {
        // printf("res_list: index = [%d], consumable = [%d], url = [%s]\n", res_list[i].index, \
        //     res_list[i].consumable, res_list[i].url);
        if (res_list[i].consumable == 1) {
            printf("Log: t2, consume response, index [%d], url [%s]\n", res_list[i].index, res_list[i].url);
            res_list[i].consumable = 2;
        }
    }
    pthread_mutex_unlock(&res_mutex);
}

// check consume finished
int check_consume_finished() {
    int i, finished;
    finished = 1;
    pthread_mutex_lock(&res_mutex);
    for (i = 0; i < NUM_URLS; ++i) {
        if (res_list[i].consumable != 2) {
            finished = 0;
            break;
        }
    }
    pthread_mutex_unlock(&res_mutex);
    return finished;
}

// t1's thread function
void *fun1() {
    printf("Log: t1 begin...\n");

    CURLM *cm;
    CURLMsg *msg;
    int msgs_left = -1;
    int still_alive = 1;
    
    curl_global_init(CURL_GLOBAL_ALL);
    cm = curl_multi_init();
    
    /* Limit the amount of simultaneous connections curl should allow: */ 
    curl_multi_setopt(cm, CURLMOPT_MAXCONNECTS, (long)MAX_PARALLEL);
    
    for(transfers = 0; transfers < MAX_PARALLEL; transfers++)
        add_transfer(cm, transfers);
    
    do {
        curl_multi_perform(cm, &still_alive);
    
        while((msg = curl_multi_info_read(cm, &msgs_left))) {
            if(msg->msg == CURLMSG_DONE) {
                struct res_info res;
                char *url;
                CURL *e = msg->easy_handle;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &url);
                strcpy(res.url, url);
                res.index = find_index_of_urls(res.url);
                add_response(res);
                printf("Log: t1, add response, error number [%d], error messsage [%s], url [%s], "
                       "index [%d]\n", msg->data.result, curl_easy_strerror(msg->data.result), res.url,  \
                        res.index);
                curl_multi_remove_handle(cm, e);
                curl_easy_cleanup(e);
            }
            else {
                printf("Log: t1, request error, error number [%d]\n", msg->msg);
            }
            if(transfers < NUM_URLS)
                add_transfer(cm, transfers++);
        }
        if(still_alive)
            curl_multi_wait(cm, NULL, 0, 1000, NULL);
    
    } while(still_alive || (transfers < NUM_URLS));
    
    curl_multi_cleanup(cm);
    curl_global_cleanup();

    printf("Log: t1 end...\n");
}

// t2's thread function
void *fun2() {
    int i = 0;
    
    printf("Log: t2 begin...\n");
    do {
        consume_response();
        sleep(1);
    } while (!check_consume_finished());
    printf("Log: t2 end...\n");
}

int main() {
    int ret, i;
    pthread_t t1, t2;

    // Initialize res_list
    for (i = 0; i < NUM_URLS; ++i) {
        res_list[i].index = -1;
        res_list[i].consumable = 0;
        strcpy(res_list[i].url, "");
    }

    // Create two thread
    ret = pthread_create(&t1, NULL, fun1, NULL);
    if (ret != 0) {
        printf("Log: Create thread t1 failed!\n");
        return -1;
    } else {
        printf("Log: Create thread t1 Successed!\n");
    }
    ret = pthread_create(&t2, NULL, fun2, NULL);
    if (ret != 0) {
        printf("Log: Create thread t2 failed!\n");
        return -1;
    } else {
        printf("Log: Create thread t2 Successed!\n");
    }

    // join thread
    ret = pthread_join(t1, NULL);
    if (ret != 0) {
        printf("Log: Join thread t1 failed!\n");
        return -1;
    } else {
        printf("Log: Join thread t1 Successed!\n");
    }
    ret = pthread_join(t2, NULL);
    if (ret != 0) {
        printf("Log: Join thread t2 failed!\n");
        return -1;
    } else {
        printf("Log: Join thread t2 Successed!\n");
    }

    printf("Log: main thread end...\n");
    return 0;
}