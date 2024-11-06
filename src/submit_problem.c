/*
 */
#include <kore/kore.h>
#include <kore/http.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <sys/types.h>
#include "gurobi_c.h"

struct problem_params {
    int amount_groups;
    int amount_locations;
    int amount_pubs;
    int max_distance;
    double ** distances;

} problem_params;

void free_problem_params(struct problem_params * params);
int solve_pub_problem(struct problem_params * params, char * filename, char* debug_string);
int get_index(int i, int j, int k, int j_size, int k_size);
int	submit_problem(struct http_request *req);
int process_integers(struct kore_buf *buf, struct kore_json *json, struct kore_json_item *item, int *processed_int, char *name);
int process_2d_int_array(struct kore_buf *buf, struct kore_json *json, struct kore_json_item *item, double ***arr, char *name);
void free_kore_resources(struct kore_buf *buf, struct kore_json *json);
/**
* Handles post requests at /submit
*
*
 */
int submit_problem(struct http_request *req){
	char *json_names[4] = {
        "problem_params/amount_groups",
        "problem_params/amount_pubs",
        "problem_params/amount_locations",
        "problem_params/max_distance"
    };

	char* json_name_distances = "problem_params/distances";
	struct kore_buf		buf;
	struct kore_json	json;
	struct kore_json_item	*item = NULL;

	int json_int_values[4] = {0}; 
	int error = 0;

	double** distances = NULL;

	kore_buf_init(&buf, 128);
	kore_json_init(&json, req->http_body->data, req->http_body->length);
	// return 400 and exit if json is invalid.
	if (kore_json_parse(&json) == KORE_RESULT_ERROR) {
		kore_buf_appendf(&buf, "%s\n", kore_json_strerror());
		http_response(req,400,buf.data,buf.offset);
		kore_buf_free(&buf);
		return KORE_RESULT_OK;
	} 

	// process the json body from the request and if it is bad input return response 400 with the error msg.
	for (int i = 0; i < 4; i++) {
        error = process_integers(&buf, &json, item, &json_int_values[i], json_names[i]);
        if (!error){
			http_response(req,400,buf.data,buf.offset);
			free_kore_resources(&buf,&json);
			return KORE_RESULT_OK;
		} // Exit loop on error
    }
	// process the distances array in the json body and return 400 with error msg if array is wrong.
	error = process_2d_int_array(&buf, &json, item, &distances,json_name_distances);
	if(!error){
		http_response(req,400,buf.data,buf.offset);
		free_kore_resources(&buf,&json);
		return KORE_RESULT_OK;
	}
	// generate a random id for the task and create the file to write the result into it.
	int r = random();
	char* filename = malloc(sizeof(char)*100);
	snprintf(filename, 100, "%i", r);
	FILE *fp = fopen(filename,"w");
	if(fp == NULL) return 0;
	fclose(fp);


	struct problem_params params;
	params.amount_groups = json_int_values[0];
	params.amount_pubs = json_int_values[1];
	params.amount_locations = json_int_values[2];
	params.max_distance = json_int_values[3];
	params.distances = distances;
	char debug_string[512];

	// #TODO this needs to be in a single program and called via command-line due to sandbox constraints or sandbox constraints need to relieved. (see seccomp in kore.io doc).
	//solve_pub_problem(&params, filename,debug_string);
	kore_buf_appendf(&buf, "%s \n", debug_string);
	kore_buf_appendf(&buf, "{'uuid': '%i'}\n",r);
	http_response(req, 200, buf.data, buf.offset);
	free_kore_resources(&buf,&json);

	return (KORE_RESULT_OK);
}

/**
* process an integer with the given json name into a number, returns 1 if sucessful, 0 if an error happened.
 */
int process_integers(struct kore_buf *buf, struct kore_json *json, struct kore_json_item *item, int *processed_int, char* name){
	item = kore_json_find_integer(json->root, name);
	if(item == NULL){
		kore_buf_appendf(buf,"%s\n",kore_json_strerror());
		return 0;
	}
	*processed_int = item->data.integer;
	return 1;
}
/**
* process an 2d integer  array with the given json name into a number, returns 1 if sucessful, 0 if an error happened.
 */
int process_2d_int_array(struct kore_buf *buf, struct kore_json *json, struct kore_json_item *item, double ***arr, char *name){
	item = kore_json_find_array(json->root,name);
	if(item == NULL){
		kore_buf_appendf(buf,"%s\n",kore_json_strerror());
		return 0;
	} 
	struct kore_json_item *element;

	int len_outer_dim = 0;
	int len_inner_dim = 0;

	TAILQ_FOREACH(element, &item->data.items, list){
		len_outer_dim++;
		struct kore_json_item *inner_element = NULL;
		len_inner_dim = 0;
		TAILQ_FOREACH(inner_element, &element->data.items, list){
			if(inner_element->type != KORE_JSON_TYPE_NUMBER) return 0;
			len_inner_dim++;
		}
	}

	*arr = (double**)malloc(sizeof(double**)*len_outer_dim);
	if(!(*arr) ) return 0;

	int i = 0;
	int j = 0;
	if(len_inner_dim == 0) return 2;
	TAILQ_FOREACH(element, &item->data.items, list){
		struct kore_json_item *inner_element;
		j = 0;
		(*arr)[i] = (double*) malloc(sizeof(double)*len_inner_dim);
		// #TODO free all the previously allocated memory if this fails.
		if(!(*arr)[i] ) return 0;
		TAILQ_FOREACH(inner_element, &element->data.items, list){
			(*arr)[i][j] = inner_element->data.number;
			j++;
		}
		i++;
	}

	return 1;
}
/*
*
*/
void free_kore_resources(struct kore_buf *buf, struct kore_json *json){
	kore_buf_free(buf);
	kore_json_cleanup(json);
}
/*
*
*/
int get_index(int i, int j, int k, int j_size, int k_size) {
    return i * (j_size * k_size) + j * k_size + k;
}


/**
 */
int solve_pub_problem(struct problem_params * params, char * filename, char* debug_string) {
    if (access(filename, F_OK) == -1){
        return 0;
    }

    GRBenv * env = NULL;
    GRBmodel * model = NULL;
    int error = 0;
    int status;
    int num_vars = params -> amount_groups * params -> amount_pubs * params -> amount_pubs;
    double sol[num_vars];
    double objval;
    // assumme that we are given a a matrix with x[I][J][K] I = Anzahl Gruppen,
    // [J] = Anzahl Positionen, K = Anzahl Kneipen.

    error = GRBemptyenv( & env);
    if (error)
        return 0;

    // creating the enviroment
    error = GRBsetstrparam(env, "logfile", "kneipe.log");
    if(error) return 0;
	snprintf(debug_string,512, "%s", GRBgeterrormsg(env));
    error = GRBstartenv(env);
    if (error)
        return 0;
	
    error =
        GRBnewmodel(env, & model, "Kneipentour", 0, NULL, NULL, NULL, NULL, NULL);
    if (error)
        return 0;
    error = GRBaddvars(model, num_vars, 0, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL);
    // dynamically allocate the memory needed and free it afterwards then...
    int iterations =
        params -> amount_groups * params -> amount_groups * (params -> amount_groups - 1) * (params -> amount_pubs - 1);
    int qrow[iterations];
    int qcol[iterations];
    double qval[iterations];

    int idx = 0;
    for (int i = 0; i < params -> amount_groups; i++) {
        for (int k = 0; k < params -> amount_groups; k++) {
            for (int h = 0; h < params -> amount_groups; h++) {
                if (h == k)
                    continue;
                for (int j = 0; j < params -> amount_pubs - 1; j++) {
                    // set j and j +1 to the distance and 1, ignore the rest.
                    qrow[idx] = get_index(i, j, k, params -> amount_pubs, params -> amount_groups);
                    qcol[idx] = get_index(i, j + 1, h, params -> amount_pubs, params -> amount_groups);
                    qval[idx] = params -> distances[k][h];
                    idx++;
                }
            }
        }
    }
    if (iterations < idx) {
        printf("error");
    }
    // now we have set variables, add to the model
    error = GRBaddqpterms(model, iterations, qrow, qcol, qval);
    if (error)
        fprintf(stderr, "Error adding quadratic terms: %s\n", GRBgeterrormsg(env));

    // add constraints.
    int * ind_pubs;
    double * val_pubs;
    // immer nur eine Kneipe besuchen.
    for (int i = 0; i < params -> amount_groups; i++) {
        for (int j = 0; j < params -> amount_pubs; j++) {
            ind_pubs = (int * ) malloc(sizeof(int) * params -> amount_groups);
            val_pubs = (double * ) malloc(sizeof(double) * params -> amount_groups);

            for (int k = 0; k < params -> amount_groups; k++) {
                val_pubs[k] = 1.0;
                ind_pubs[k] = get_index(i, j, k, params -> amount_pubs, params -> amount_groups);
            }
            GRBaddconstr(model, params -> amount_groups, ind_pubs, val_pubs, GRB_EQUAL, 1.0,
                "Exakt eine Kneipe pro Zeitpunkt");
            free(ind_pubs);
            free(val_pubs);
        }
    }
    // jede Kneipe nur einmal
    int * ind_only_one_visit;
    double * val_only_one_visit;
    for (int i = 0; i < params -> amount_groups; i++) {
        for (int k = 0; k < params -> amount_groups; k++) {
            ind_only_one_visit = (int * ) malloc(sizeof(int) * params -> amount_pubs);
            val_only_one_visit = (double * ) malloc(sizeof(double) * params -> amount_pubs);
            for (int j = 0; j < params -> amount_pubs; j++) {
                ind_only_one_visit[j] = get_index(i, j, k, params -> amount_pubs, params -> amount_groups);
                val_only_one_visit[j] = 1.0;
            }
            GRBaddconstr(model, params -> amount_pubs, ind_only_one_visit, val_only_one_visit,
                GRB_LESS_EQUAL, 1.0, "Jede Kneipe nur einmal");
            free(ind_only_one_visit);
            free(val_only_one_visit);
        }
    }
    // jede Kneipe wird zu jedem Zeitpunkt von mindestens eienr Gruppe besucht.
    for (int i = 0; i < params -> amount_groups; i++) {
        for (int j = 0; j < params -> amount_pubs; j++) {
            int * ind_always_visited = (int * ) malloc(sizeof(int) * params -> amount_groups);
            double * val_always_visited =
                (double * ) malloc(sizeof(double) * params -> amount_groups);
            for (int k = 0; k < params -> amount_groups; k++) {
                ind_always_visited[k] = get_index(i, j, k, params -> amount_pubs, params -> amount_groups);
                val_always_visited[k] = 1.0;
            }
            GRBaddconstr(model, params -> amount_groups, ind_always_visited, val_always_visited,
                GRB_GREATER_EQUAL, 1.0,
                "Jede Kneipe besucht zu jedem Zeitpunkt.");
            free(ind_always_visited);
            free(val_always_visited);
        }
    }
    // Limit Constraints.
    for (int k = 0; k < params -> amount_groups; k++) {
        for (int j = 0; j < params -> amount_pubs; j++) {
            int * limit_ind = (int * ) malloc(sizeof(int) * params -> amount_groups);
            double * limit_val = (double * ) malloc(sizeof(double) * params -> amount_groups);
            for (int i = 0; i < params -> amount_groups; i++) {
                limit_ind[i] = get_index(i, j, k, params -> amount_pubs, params -> amount_groups);
                limit_val[i] = 1.0;
            }
            GRBaddconstr(model, params -> amount_groups, limit_ind, limit_val, GRB_LESS_EQUAL,
                1, "Gruppenlimit pro Kneipe");
            free(limit_ind);
            free(limit_val);
        }
    }
    // quadratische Distanzconstraints...
    for (int i = 0; i < params -> amount_groups; i++) {
        for (int k = 0; k < params -> amount_groups; k++) {
            for (int h = 0; h < params -> amount_groups; h++) {
                if (h == k)
                    continue;
                for (int j = 0; j < params -> amount_pubs - 1; j++) {
                    // set j and j +1 to the distance and 1, ignore the rest.
                    int row[] = {
                        get_index(i, j, k, params -> amount_pubs, params -> amount_groups)
                    };
                    int col[] = {
                        get_index(i, j + 1, k, params -> amount_pubs, params -> amount_groups)
                    };
                    double val[] = {
                        params -> distances[k][h]
                    };
                    GRBaddqconstr(model, 0, NULL, NULL, 1, row, col, val, GRB_LESS_EQUAL,
                        params -> max_distance, "nicht zu lange gehen");
                }
            }
        }
    }
    // set objective to maximizing
    error = GRBsetintattr(model, GRB_INT_ATTR_MODELSENSE, GRB_MAXIMIZE);
    if (error)
        return 0;
    error = GRBoptimize(model);
    if (error)
        return 0;
    error = GRBgetdblattr(model, GRB_DBL_ATTR_OBJVAL, & objval);
	GRBfreemodel(model);
	GRBfreeenv(env);
	// #TODO free the double distances...
    return 0;

}

void free_problem_params(struct problem_params *params) {
    // 1. Free the individual rows of the distances array
    for (int i = 0; i < params->amount_locations; i++) {
        free(params->distances[i]);
    }

    // 2. Free the array of pointers (distances)
    free(params->distances);

    // 3. Free the struct itself
    free(params);
}
