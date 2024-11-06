#include <kore/kore.h>
#include <kore/http.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <uuid/uuid.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <sys/types.h>

int	get_result(struct http_request *req);



int	get_result(struct http_request *req){
    struct kore_buf		*buf;
    int uuid;
    struct stat file_stat;
    http_populate_get(req);
    buf = kore_buf_alloc(64);
    
    /* Grab it as a string, we shouldn't free the result in sid. */
	if (!http_argument_get_int32(req, "id", &uuid)){
        http_response(req, 400, buf->data, buf->offset);
        return KORE_RESULT_OK;
    }
    char filename[21];
    snprintf(filename, 21, "%i",uuid);
    kore_buf_appendf(buf, "./%s\n",filename);

    if(access(filename,F_OK) != 0){
        kore_buf_appendf(buf, "file not found",NULL);
        http_response(req, 400, buf->data, buf->offset);
        kore_buf_free(buf);
        return KORE_RESULT_OK;
    }
    
    if (stat(filename, &file_stat) == 0) {
        if (file_stat.st_size == 0) 
        {
            
            http_response(req,202,buf->data,buf->offset);
            kore_buf_free(buf);
            return KORE_RESULT_OK;
        }
    }else{
        http_response(req, 400, buf->data, buf->offset);
        kore_buf_free(buf);
        return KORE_RESULT_OK;
    }
    FILE *fp; 
    fp = fopen(filename, "r");
    char line[100];
    while (fgets(line, 100, fp)) {
        kore_buf_appendf(buf, "%s\n",line);
    }
    fclose(fp);
    http_response(req,200,buf->data,buf->offset);
    kore_buf_free(buf);
    return KORE_RESULT_OK;

}