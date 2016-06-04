int main(int argc, char *argv[])
{
    int x;

    /* With curlies */
    for (int i = 0; i < 10; i++) {
        x = 0;
    }
    do {
        x = 0;
    } while (0);

    while (0) {
        x = 0;
    }

    /* Without curlies */
    for (int i = 0; i < 10; i++)
        x = 0;
    do
        x = 0;
    while (0);

    while (0)
        x = 0;
}
