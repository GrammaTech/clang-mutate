char *var;

#define INNER(s) s
#define OUTER(s) INNER(s)
#define VAR var

int main() {
    INNER(var);
    OUTER(var);
    OUTER(VAR);
}
