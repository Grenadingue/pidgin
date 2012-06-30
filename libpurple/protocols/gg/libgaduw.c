#include "libgaduw.h"

#include <debug.h>

#include "purplew.h"

/*******************************************************************************
 * HTTP requests.
 ******************************************************************************/

static void ggp_libgaduw_http_handler(gpointer _req, gint fd,
	PurpleInputCondition cond);

static void ggp_libgaduw_http_cb_call(ggp_libgaduw_http_req *req,
	gboolean success);

/******************************************************************************/

ggp_libgaduw_http_req * ggp_libgaduw_http_watch(struct gg_http *h,
	ggp_libgaduw_http_cb cb, gpointer user_data)
{
	ggp_libgaduw_http_req *req = g_new(ggp_libgaduw_http_req, 1);
	purple_debug_info("gg", "ggp_libgaduw_http_watch\n");
	
	req->user_data = user_data;
	req->cb = cb;
	req->h = h;
	req->inpa = purple_input_add(h->fd, PURPLE_INPUT_READ,
		ggp_libgaduw_http_handler, req);
	
	return req;
}

static void ggp_libgaduw_http_handler(gpointer _req, gint fd,
	PurpleInputCondition cond)
{
	ggp_libgaduw_http_req *req = _req;
	
	if (gg_token_watch_fd(req->h) == -1 || req->h->state == GG_STATE_ERROR)
	{
		purple_debug_error("gg", "ggp_libgaduw_http_handler: failed to "
			"make http request: %d\n", req->h->error);
		ggp_libgaduw_http_cb_call(req, FALSE);
		return;
	}
	
	//TODO: verbose mode
	purple_debug_misc("gg", "ggp_libgaduw_http_handler: got fd update "
		"[check=%d, state=%d]\n", req->h->check, req->h->state);
	
	if (req->h->state != GG_STATE_DONE)
	{
		purple_input_remove(req->inpa);
		req->inpa = ggp_purplew_http_input_add(req->h,
			ggp_libgaduw_http_handler, req);
		return;
	}
	
	if (!req->h->data || !req->h->body)
	{
		purple_debug_error("gg", "ggp_libgaduw_http_handler: got empty "
			"http response: %d\n", req->h->error);
		ggp_libgaduw_http_cb_call(req, FALSE);
		return;
	}

	ggp_libgaduw_http_cb_call(req, TRUE);
}

static void ggp_libgaduw_http_cb_call(ggp_libgaduw_http_req *req,
	gboolean success)
{
	purple_input_remove(req->inpa);
	req->cb(req->h, success, req->user_data);
	g_free(req);
}

void ggp_libgaduw_http_cancel(ggp_libgaduw_http_req *req)
{
	purple_debug_info("gg", "ggp_libgaduw_http_cancel\n");
	gg_http_stop(req->h);
	ggp_libgaduw_http_cb_call(req, FALSE);
}
