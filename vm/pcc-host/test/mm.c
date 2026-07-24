#define BUSY 1
#define testbusy(p) ((p).flag & BUSY)
struct store { struct store *ptr; unsigned flag; };
static struct store *allocp;
int f() {
    struct store *p;
    p = allocp;
    if (!testbusy(*p)) return 1;
    return 0;
}
