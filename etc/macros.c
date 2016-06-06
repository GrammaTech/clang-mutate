#define SHORT_MACRO 0
#define LONG_MACRO  123456789
#define MULTI_STATEMENT_1 if (0) 1;
#define MULTI_STATEMENT_2(n) do { 1; } while (0)

int main(void)
{
    MULTI_STATEMENT_1;

    MULTI_STATEMENT_2(foo);

    return SHORT_MACRO + LONG_MACRO;
}
