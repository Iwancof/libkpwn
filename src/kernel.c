#include <kpwn/kernel.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <stdarg.h>

void* kbase = NULL;

void set_kbase(void* addr) {
  log_success("setting kernel base address to %p", addr);
  kbase = addr;
}

size_t call_ptr(void* fptr, int argc, va_list list) {
  size_t args[argc];
  for (int i = 0; i < argc; i++)
    args[i] = va_arg(list, size_t);

  switch (argc) {
    case 0: {
      size_t (*ft)() = fptr;
      return ft();
    }
    case 1: {
      size_t (*ft)(size_t a) = fptr;
      return ft(args[0]);
    }
    case 2: {
      size_t (*ft)(size_t a, size_t b) = fptr;
      return ft(args[0], args[1]);
    }
    case 3: {
      size_t (*ft)(size_t a, size_t b, size_t c) = fptr;
      return ft(args[0], args[1], args[2]);
    }
    case 4: {
      size_t (*ft)(size_t a, size_t b, size_t c, size_t d) = fptr;
      return ft(args[0], args[1], args[2], args[3]);
    }
    case 5: {
      size_t (*ft)(size_t a, size_t b, size_t c, size_t d, size_t e) = fptr;
      return ft(args[0], args[1], args[2], args[3], args[4]);
    }
    case 6: {
      size_t (*ft)(size_t a, size_t b, size_t c, size_t d, size_t e, size_t f) = fptr;
      return ft(args[0], args[1], args[2], args[3], args[4], args[5]);
    }
    default: {
      log_error("call_ptr: unsupported argument count %d", argc);
      ASSERT(0);
      return 0;  // unreachable
    }
  }
}

size_t kfunc_abs(void* fptr, int argc, ...) {
  va_list list;
  va_start(list, argc);

  size_t ret = call_ptr(fptr, argc, list);
  va_end(list);

  return ret;
}

size_t kfunc_off(void* fptr, int argc, ...) {
  va_list list;
  va_start(list, argc);

  if (kbase == NULL) {
    log_error(
        "kbase is not initialized. you probably forget "
        "initialize or use kfunc_abs instead of kfunc_off");
    ASSERT(0);
  }

  size_t ret = call_ptr(kbase + (size_t)fptr, argc, list);
  va_end(list);

  return ret;
}
