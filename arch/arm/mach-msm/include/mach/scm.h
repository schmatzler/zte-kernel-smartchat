/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __MACH_SCM_H
#define __MACH_SCM_H

#define SCM_SVC_BOOT			0x1
#define SCM_SVC_PIL			0x2
#define SCM_SVC_UTIL			0x3
#define SCM_SVC_TZ			0x4
#define SCM_SVC_IO			0x5
#define SCM_SVC_INFO			0x6
#define SCM_SVC_TZSCHEDULER		0xFC

#ifdef CONFIG_MSM_SCM
extern int scm_call(u32 svc_id, u32 cmd_id, const void *cmd_buf, size_t cmd_len,
		void *resp_buf, size_t resp_len);

extern u32 scm_call_atomic1(u32 svc, u32 cmd, u32 arg1);
extern u32 scm_call_atomic2(u32 svc, u32 cmd, u32 arg1, u32 arg2);

#define SCM_VERSION(major, minor) (((major) << 16) | ((minor) & 0xFF))

extern u32 scm_get_version(void);
extern int scm_is_call_available(u32 svc_id, u32 cmd_id);

#else

static inline int scm_call(u32 svc_id, u32 cmd_id, const void *cmd_buf,
		size_t cmd_len, void *resp_buf, size_t resp_len)
{
	return 0;
}

static inline u32 scm_call_atomic1(u32 svc, u32 cmd, u32 arg1)
{
	return 0;
}

static inline u32 scm_call_atomic2(u32 svc, u32 cmd, u32 arg1, u32 arg2)
{
	return 0;
}

static inline u32 scm_get_version(void)
{
	return 0;
}

static inline int scm_is_call_available(u32 svc_id, u32 cmd_id)
{
	return 0;
}

#endif
#endif
