enum tree_code {
    SOME_CODE = 148,
    LAST_AND_UNUSED_TREE_CODE
};
typedef union tree_node *tree;
struct tree_common {
    union tree_node *chain;
    union tree_node *type;
    enum tree_code code : 8;
    unsigned side_effects_flag : 1;
};
union tree_node {
    struct tree_common common;
};
enum c_tree_code {
    C_DUMMY_TREE_CODE = LAST_AND_UNUSED_TREE_CODE,
    STMT_EXPR,
    LAST_C_TREE_CODE
};
enum cplus_tree_code {
    CP_DUMMY_TREE_CODE = LAST_C_TREE_CODE,
    AMBIG_CONV,
    LAST_CPLUS_TREE_CODE
};

extern int printf(const char *, ...);
int blah() {
    return 0;
}

int convert_like_real(tree convs) {
    switch (((enum tree_code)(convs)->common.code)) {
    case AMBIG_CONV:

        return blah();
    default:
        break;
    };
    printf("unsigned enum bit-fields broken\n");
    return 0;
}

int main() {
    union tree_node convs;

    convs.common.code = AMBIG_CONV;
    convert_like_real(&convs);
    return 0;
}
