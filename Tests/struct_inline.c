int printf(const char *, ...);

int main(int argc, char **argv)
{
  struct KeyValue {
    const char *key;
    int value;
  } kv;
  printf("size = %d\n", sizeof(kv));
  return 0;
}
