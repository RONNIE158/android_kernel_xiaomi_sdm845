#ifndef _UAPI_LINUX_OPENAT2_H
#define _UAPI_LINUX_OPENAT2_H
struct open_how {
  __u64 flags;
  __u64 mode;
  __u64 resolve;
};
#endif
