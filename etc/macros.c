#define SHORT_MACRO 0
#define LONG_MACRO  123456789
#define MULTI_STATEMENT_MACRO if (0) 1;

int main(void)
{
    MULTI_STATEMENT_MACRO;

    return SHORT_MACRO + LONG_MACRO;
}
