/*
 * TUX - Integrated Application Protocols Layer and Object Cache
 *
 * Copyright (C) 2000, 2001, Ingo Molnar <mingo@redhat.com>
 *
 * extcgi.c: dynamic TUX module which forks and starts an external CGI
 */

#define __KERNEL_SYSCALLS__

#include <net/tux.h>
#include "parser.h"

/****************************************************************
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2, or (at your option)
 *      any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ****************************************************************/

#define MAX_ENVLEN 1000
#define MAX_CGI_METAVARIABLES 32
#define CGI_CHUNK_SIZE 1024 
#define MAX_CGI_COMMAND_LEN 256

#if CONFIG_TUX_DEBUG
#define PRINT_MESSAGE_LEFT \
	Dprintk("CGI message left at %s:%d:\n--->{%s}<---\n", \
		__FILE__, __LINE__, curr)
#else
#define PRINT_MESSAGE_LEFT do {} while(0)
#endif

#define GOTO_INCOMPLETE do { Dprintk("invalid CGI reply at %s:%d.\n", __FILE__, __LINE__); goto invalid; } while (0)

/*
 * Please acknowledge our hard work by not changing this define, or
 * at least please acknowledge us by leaving "TUX/2.0 (Linux)" in
 * the ID string. Thanks! :-)
 */
#define CGI_SUCCESS2 "HTTP/1.1 200 OK\r\nConnection: close\r\nServer: TUX/2.0 (Linux)\r\n"

static int handle_cgi_reply (tux_req_t *req)
{
	int first = 1;
	int len, left, total;
	char buf [CGI_CHUNK_SIZE+1], *tmp;
	mm_segment_t oldmm;

	sys_close(3);
	sys_close(4);
	sys_close(5);
	oldmm = get_fs(); set_fs(KERNEL_DS);
	send_sync_buf(NULL, req->sock, CGI_SUCCESS2, sizeof(CGI_SUCCESS2)-1, MSG_MORE);
	set_fs(oldmm);

	req->bytes_sent = 0;
	/*
	 * The new process is the new owner of the socket, it will
	 * close it.
	 */
repeat:
	left = CGI_CHUNK_SIZE;
	len = 0;
	total = 0;
	tmp = buf;
	do {
		mm_segment_t oldmm;

		tmp += len;
		total += len;
		left -= len;
		if (!left)
			break;
repeat_read:
		Dprintk("reading %d bytes via sys_read().\n", left);
		oldmm = get_fs(); set_fs(KERNEL_DS);
		len = sys_read(2, tmp, left);
		set_fs(oldmm);
		Dprintk("got %d bytes from sys_read() (total: %d).\n", len, total);
		if (len > 0)
			tmp[len] = 0;
		Dprintk("CGI reply: (%d bytes, total %d).\n", len, total);
		if (len == -ERESTARTSYS) {
			flush_all_signals();
			reap_kids();
			goto repeat_read;
		}
	} while (len > 0);
	if (total > CGI_CHUNK_SIZE) {
		printk(KERN_ERR "TUX: CGI weirdness. total: %d, len: %d, left: %d.\n", total, len, left);
		TUX_BUG();
	}
	Dprintk("CGI done reply chunk: (%d bytes last, total %d).\n", len, total);
	if (total) {
		mm_segment_t oldmm;

		oldmm = get_fs(); set_fs(KERNEL_DS);
		if (!len)
			send_sync_buf(NULL, req->sock, buf, total, 0);
		else
			send_sync_buf(NULL, req->sock, buf, total, MSG_MORE);
		set_fs(oldmm);
		req->bytes_sent += total;
	}

	Dprintk("bytes_sent: %d\n", req->bytes_sent);
	if ((total > 0) && first) {
		first = 0;

		if (buf[total])
			TUX_BUG();
		tmp = strstr(buf, "\n\n");
		if (tmp) {
			req->bytes_sent -= (tmp-buf) + 2;
			Dprintk("new bytes_sent: %d\n", req->bytes_sent);
		} else {
			req->bytes_sent = 0;
			req_err(req);
		}
	}
	if (len < 0)
		Dprintk("sys_read returned with %d.\n", len);
	else {
		if (total > 0)
			goto repeat;
	}
	sys_close(2);

	req->status = 200;
	add_req_to_workqueue(req);
	return -1;
}

static int exec_external_cgi (void *data)
{
	exec_param_t param;
	tux_req_t *req = data;
	char *envp[MAX_CGI_METAVARIABLES+1], **envp_p;
	char *argv[] = { "extcgi", NULL};
	char envstr[MAX_ENVLEN], *tmp;
	unsigned int host;
	struct k_sigaction *ka;
	int in_pipe_fds[2], out_pipe_fds[2], err_pipe_fds[2], len;
	char command [MAX_CGI_COMMAND_LEN];
	pid_t pid;
	int ret;

	len = strlen(tux_common_docroot);
	if (req->objectname_len + len + 12 > MAX_CGI_COMMAND_LEN)
		return -ENOMEM;
	sprintf(current->comm,"cgimain - %d", current->pid);
#define IP(x) (((unsigned char *)&host)[x])
	host = req->sock->sk->daddr;

	tmp = envstr;
	envp_p = envp;

#define WRITE_ENV(str...) \
	if (envp_p >= envp + MAX_CGI_METAVARIABLES) \
		TUX_BUG(); \
	len = sprintf(tmp, str); \
	*envp_p++ = tmp; \
	tmp += len + 1; \
	if (tmp >= envstr + MAX_ENVLEN) \
		TUX_BUG();

	#define WRITE_ENV_STR(str,field,len)			\
	do {							\
		int offset;					\
								\
		offset = sizeof(str)-1;				\
		if (tmp - envstr + offset + len >= MAX_ENVLEN)	\
			return -EFAULT;				\
		if (envp_p >= envp + MAX_CGI_METAVARIABLES) 	\
			TUX_BUG(); 				\
		memcpy(tmp, str, offset);			\
		memcpy(tmp + offset, field, len);		\
		offset += len;					\
		tmp[offset] = 0;				\
		*envp_p++ = tmp;				\
		tmp += offset + 1;				\
	} while (0)

	WRITE_ENV("GATEWAY_INTERFACE=CGI/1.1");
	WRITE_ENV("CONTENT_LENGTH=%d", req->post_data_len);
	WRITE_ENV("REMOTE_ADDR=%d.%d.%d.%d", IP(0), IP(1), IP(2), IP(3));
	WRITE_ENV("SERVER_PORT=%d", 80);
	WRITE_ENV("SERVER_SOFTWARE=TUX/2.0 (Linux)");

#if 1
	WRITE_ENV("DOCUMENT_ROOT=/");
	WRITE_ENV("PATH_INFO=/");
#else
	WRITE_ENV_STR("DOCUMENT_ROOT=", tux_common_docroot, len);
	WRITE_ENV_STR("PATH_INFO=", tux_common_docroot, len);
#endif
	WRITE_ENV_STR("QUERY_STRING=", req->query_str, req->query_len);
	WRITE_ENV_STR("REQUEST_METHOD=", req->method_str, req->method_len);
	WRITE_ENV_STR("SCRIPT_NAME=", req->objectname, req->objectname_len);
	WRITE_ENV_STR("SERVER_PROTOCOL=", req->version_str, req->version_len);

	if (req->content_type_len)
		WRITE_ENV_STR("CONTENT_TYPE=",
			req->content_type_str, req->content_type_len);
	if (req->cookies_len)
		WRITE_ENV_STR("HTTP_COOKIE=",
			req->cookies_str, req->cookies_len);

	if (req->host_len)
		WRITE_ENV_STR("SERVER_NAME=", req->host, req->host_len);
	else {
		const char *host = "localhost";
		WRITE_ENV_STR("SERVER_NAME=", host, strlen(host));
	}

	*envp_p = NULL;

	spin_lock_irq(&current->sigmask_lock);
	ka = current->sig->action + SIGPIPE-1;
	ka->sa.sa_handler = SIG_IGN;
	siginitsetinv(&current->blocked, sigmask(SIGCHLD));
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	sys_close(0); sys_close(1);
	sys_close(2); sys_close(3);
	sys_close(4); sys_close(5);

	in_pipe_fds[0] = in_pipe_fds[1] = -1;
	out_pipe_fds[0] = out_pipe_fds[1] = -1;
	err_pipe_fds[0] = err_pipe_fds[1] = -1;

	if (do_pipe(in_pipe_fds))
		return -ENFILE;
	if (do_pipe(out_pipe_fds))
		return -ENFILE;
	if (do_pipe(err_pipe_fds))
		return -ENFILE;

	if (in_pipe_fds[0] != 0) TUX_BUG();
	if (in_pipe_fds[1] != 1) TUX_BUG();
	if (out_pipe_fds[0] != 2) TUX_BUG();
	if (out_pipe_fds[1] != 3) TUX_BUG();
	if (err_pipe_fds[0] != 4) TUX_BUG();
	if (err_pipe_fds[1] != 5) TUX_BUG();

	if (req->virtual && req->host_len)
		sprintf(command, "/%s/cgi-bin/%s", req->host, req->objectname);
	else
		sprintf(command, "/cgi-bin/%s", req->objectname);
	Dprintk("before CGI exec.\n");
	pid = tux_exec_process(command, argv, envp, 1, &param, 0);
	Dprintk("after CGI exec.\n");

	if (req->post_data_len) {
		mm_segment_t oldmm;
		int ret;

		Dprintk("POST data to CGI:\n");
		oldmm = get_fs(); set_fs(KERNEL_DS);
		ret = sys_write(1, req->post_data_str, req->post_data_len);
		set_fs(oldmm);
		Dprintk("sys_write() returned: %d.\n", ret);
		if (ret != req->post_data_len)
			Dprintk("sys_write() returned: %d.\n", ret);
	}

	sys_close(0);
	sys_close(1);

	handle_cgi_reply(req);
repeat:
	reap_kids();
	ret = sys_wait4(pid, NULL, __WALL, NULL);
	Dprintk("exec_external_cgi() sys_wait4() returned %d.\n", ret);
	if (ret == -ERESTARTSYS)
		goto repeat;

	return ret;
}

void start_external_cgi (tux_req_t *req)
{
	int pid;

repeat:
	pid = kernel_thread(exec_external_cgi, (void*) req, SIGCHLD);
	if (pid == -1)
		return;
	if (pid < 0) {
		printk(KERN_INFO "TUX: Could not fork external CGI process due to %d, retrying!\n", pid);
		current->state = TASK_UNINTERRUPTIBLE;
		schedule_timeout(HZ);
		goto repeat;
	}
}

int query_extcgi (tux_req_t *req)
{
	clear_keepalive(req);
	start_external_cgi(req);
	return -1;
}

#define EXTCGI_INVALID_HEADER \
	"HTTP/1.1 503 Service Unavailable\r\n" \
	"Content-Length: 23\r\n\r\n"

#define EXTCGI_INVALID_BODY \
	"TUX: invalid CGI reply."

#define EXTCGI_INVALID EXTCGI_INVALID_HEADER EXTCGI_INVALID_BODY

