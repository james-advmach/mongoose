#include "fmt.h"
#include "iobuf.h"
#include "queue.h"
#include "util.h"

size_t mg_queue_vprintf(struct mg_queue *q, const char *fmt, va_list *ap) {
  size_t len = mg_snprintf(NULL, 0, fmt, ap);
  char *buf;
  if (len == 0 || mg_queue_book(q, &buf, len + 1) < len + 1) {
    len = 0;  // Nah. Not enough space
  } else {
    len = mg_vsnprintf((char *)buf, len + 1, fmt, ap);
    mg_queue_add(q, len);
  }
  return len;
}

size_t mg_queue_printf(struct mg_queue *q, const char *fmt, ...) {
  va_list ap;
  size_t len;
  va_start(ap, fmt);
  len = mg_queue_vprintf(q, fmt, &ap);
  va_end(ap);
  return len;
}

static void mg_pfn_iobuf_private(char ch, void *param, bool expand) {
  struct mg_iobuf *io = (struct mg_iobuf *) param;
  if (expand && io->len + 2 > io->size) mg_iobuf_resize(io, io->len + 2);
  if (io->len + 2 <= io->size) {
    io->buf[io->len++] = (uint8_t) ch;
    io->buf[io->len] = 0;
  } else if (io->len < io->size) {
    io->buf[io->len++] = 0;  // Guarantee to 0-terminate
  }
}

static void mg_putchar_iobuf_static(char ch, void *param) {
  mg_pfn_iobuf_private(ch, param, false);
}

void mg_pfn_iobuf(char ch, void *param) {
  mg_pfn_iobuf_private(ch, param, true);
}

size_t mg_vsnprintf(char *buf, size_t len, const char *fmt, va_list *ap) {
  struct mg_iobuf io = {(uint8_t *) buf, len, 0, 0};
  size_t n = mg_vxprintf(mg_putchar_iobuf_static, &io, fmt, ap);
  if (n < len) buf[n] = '\0';
  return n;
}

size_t mg_snprintf(char *buf, size_t len, const char *fmt, ...) {
  va_list ap;
  size_t n;
  va_start(ap, fmt);
  n = mg_vsnprintf(buf, len, fmt, &ap);
  va_end(ap);
  return n;
}

char *mg_vmprintf(const char *fmt, va_list *ap) {
  struct mg_iobuf io = {0, 0, 0, 256};
  mg_vxprintf(mg_pfn_iobuf, &io, fmt, ap);
  return (char *) io.buf;
}

char *mg_mprintf(const char *fmt, ...) {
  char *s;
  va_list ap;
  va_start(ap, fmt);
  s = mg_vmprintf(fmt, &ap);
  va_end(ap);
  return s;
}

void mg_pfn_stdout(char c, void *param) {
  putchar(c);
  (void) param;
}

static size_t print_ip4(void (*out)(char, void *), void *arg, uint8_t *p) {
  return mg_xprintf(out, arg, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
}

static size_t print_ip6(void (*out)(char, void *), void *arg, uint16_t *p) {
  return mg_xprintf(out, arg, "[%x:%x:%x:%x:%x:%x:%x:%x]", mg_ntohs(p[0]),
                    mg_ntohs(p[1]), mg_ntohs(p[2]), mg_ntohs(p[3]),
                    mg_ntohs(p[4]), mg_ntohs(p[5]), mg_ntohs(p[6]),
                    mg_ntohs(p[7]));
}

size_t mg_print_ip4(void (*out)(char, void *), void *arg, va_list *ap) {
  uint8_t *p = va_arg(*ap, uint8_t *);
  return print_ip4(out, arg, p);
}

size_t mg_print_ip6(void (*out)(char, void *), void *arg, va_list *ap) {
  uint16_t *p = va_arg(*ap, uint16_t *);
  return print_ip6(out, arg, p);
}

size_t mg_print_ip(void (*out)(char, void *), void *arg, va_list *ap) {
  struct mg_addr *addr = va_arg(*ap, struct mg_addr *);
  if (addr->is_ip6) return print_ip6(out, arg, (uint16_t *) addr->ip6);
  return print_ip4(out, arg, (uint8_t *) &addr->ip);
}

size_t mg_print_ip_port(void (*out)(char, void *), void *arg, va_list *ap) {
  struct mg_addr *a = va_arg(*ap, struct mg_addr *);
  return mg_xprintf(out, arg, "%M:%hu", mg_print_ip, a, mg_ntohs(a->port));
}

size_t mg_print_mac(void (*out)(char, void *), void *arg, va_list *ap) {
  uint8_t *p = va_arg(*ap, uint8_t *);
  return mg_xprintf(out, arg, "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2],
                    p[3], p[4], p[5]);
}
