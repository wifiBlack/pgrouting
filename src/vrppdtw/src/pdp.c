/*PGR-GNU*****************************************************************

Copyright (c) 2014 Manikata Kondeti
mani.iiit123@gmail.com

------

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

********************************************************************PGR-GNU*/
#include "postgres.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "catalog/pg_type.h"
#if PGSQL_VERSION > 92
#include "access/htup_details.h"
#endif

#include "fmgr.h"


PG_FUNCTION_INFO_V1(vrppdtw);
#ifndef _MSC_VER
Datum
#else  // _MSC_VER
PGDLLEXPORT Datum
#endif
vrppdtw(PG_FUNCTION_ARGS);


#include "./pdp_solver.h"

#define DEBUG 
#include "../../common/src/debug_macro.h"
#include "../../common/src/postgres_connection.h"
#include "./customers_input.h"




static
int compute_shortest_path(
        char* sql,
        int64_t vehicle_count,
        double capacity,
        path_element **results,
        size_t *length_results_struct) {
    PGR_DBG("start shortest_path\n");

    pgr_SPI_connect();
    char *err_msg = NULL;
    size_t total_customers = 0;
    Customer *customers = NULL;
    PGR_DBG("Calling pgr_get_customers\n");
    pgr_get_customers(sql, &customers, &total_customers);
    return 0;
    PGR_DBG("Calling Solver Instance\n");
#if 0
    size_t i;
    for (i = 0; i < total_customers ; ++i) {
        PGR_DBG("%zu: %ld\t %f\t%f\t%f\t %f\t%f\t%f\t %ld\t%ld\t  %f", i,
                customers[i].id,
                customers[i].x,
                customers[i].y,
                customers[i].demand,
                customers[i].Etime,
                customers[i].Ltime,
                customers[i].Stime,
                customers[i].Pindex,
                customers[i].Dindex,
                customers[i].Ddist
               );
    }
#endif

    int64_t ret = Solver(customers, total_customers, vehicle_count,
            capacity, &err_msg, results, length_results_struct);

    if (err_msg) PGR_DBG("%s\n",err_msg);
#if 0
    if (ret < 0) {
        ereport(ERROR, (errcode(ERRCODE_E_R_E_CONTAINING_SQL_NOT_PERMITTED),
                    errmsg("Error computing path: %s", err_msg)));
    }
#endif

    PGR_DBG("*length_results_count  = %ld\n", *length_results_struct);
    PGR_DBG("ret = %ld\n", ret);

    pfree(customers);
    if (err_msg) free(err_msg);
    pgr_SPI_finish();
    return 0;
}



#ifndef _MSC_VER
Datum
#else  // _MSC_VER
PGDLLEXPORT Datum
#endif
vrppdtw(PG_FUNCTION_ARGS) {
    FuncCallContext     *funcctx;
    uint32_t               call_cntr;
    uint32_t               max_calls;
    TupleDesc            tuple_desc;
    path_element     *results = NULL;


    /* stuff done only on the first call of the function */

    if (SRF_IS_FIRSTCALL()) {
        MemoryContext   oldcontext;
        size_t length_results_struct = 0;
        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
        //results = (path_element *)palloc(sizeof(path_element)*((length_results_struct) + 1));

        PGR_DBG("Calling compute_shortes_path");

        compute_shortest_path(
                pgr_text2char(PG_GETARG_TEXT_P(0)),  // customers sql
                PG_GETARG_INT64(1),  // vehicles  count
                PG_GETARG_FLOAT8(2),  // capacity 
                &results, &length_results_struct);

        PGR_DBG("Back from solve_vrp, length_results: %ld", length_results_struct);

        /* total number of tuples to be returned */
        funcctx->max_calls = (uint32_t)length_results_struct;
        funcctx->user_fctx = results;

        /* Build a tuple descriptor for our result type */
        if (get_call_result_type(fcinfo, NULL, &tuple_desc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("function returning record called in context "
                         "that cannot accept type record")));

        funcctx->tuple_desc = BlessTupleDesc(tuple_desc);
        MemoryContextSwitchTo(oldcontext);
    }

    /* stuff done on every call of the function */
    funcctx = SRF_PERCALL_SETUP();

    call_cntr = funcctx->call_cntr;
    max_calls = funcctx->max_calls;
    tuple_desc = funcctx->tuple_desc;
    results = (path_element *) funcctx->user_fctx;

    /* do when there is more left to send */
    if (call_cntr < max_calls) {
        HeapTuple    tuple;
        Datum        result;
        Datum *values;
        char* nulls;

        PGR_DBG("Till hereee ");
        values = palloc(4 * sizeof(Datum));
        nulls = palloc(4 * sizeof(char));

        nulls[0] = ' ';
        nulls[1] = ' ';
        nulls[2] = ' ';
        nulls[3] = ' ';
        values[0] = Int32GetDatum(results[call_cntr].seq);
        values[1] = Int64GetDatum(results[call_cntr].rid);
        values[2] = Int64GetDatum(results[call_cntr].nid);
        values[3] = Float8GetDatum(results[call_cntr].cost);
        tuple = heap_formtuple(tuple_desc, values, nulls);

        /* make the tuple into a datum */
        result = HeapTupleGetDatum(tuple);

        /* clean up (this is not really necessary) */
        // pfree(values);
        // pfree(nulls);

        SRF_RETURN_NEXT(funcctx, result);
    } else {
        /* do when there is no more left */
        free(results);
        SRF_RETURN_DONE(funcctx);
    }
}
