#include <stdio.h>
/*
 * No changes are allowed in this file
 */
int fib(int n) {
  if(n<2) return n;
  else return fib(n-1)+fib(n-2);
}
int main() {
    int n;
    scanf("%d",&n);
    int val = fib(n);
    printf("Answer is '%d'\n",val);
}