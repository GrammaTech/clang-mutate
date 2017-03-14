

#define __GMP_PROTO(x) x

typedef void (*dss_func) __GMP_PROTO ((int));

void
mpz_xinvert (int x)
{
  int res;
  res = x + 5;
}

dss_func dss_funcs[] =
{
  mpz_xinvert
};
