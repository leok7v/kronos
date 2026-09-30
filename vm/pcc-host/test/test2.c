int arr[10];
int fib(n) int n; {
    if (n < 2) return n;
    return fib(n-1) + fib(n-2);
}
int sum() {
    int i, s;
    s = 0;
    for (i = 0; i < 10; i = i + 1) s = s + arr[i];
    return s;
}
