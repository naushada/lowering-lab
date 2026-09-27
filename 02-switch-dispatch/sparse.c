/* Sparse cases: a jump table would need a million entries, so the compiler
   builds a decision tree of compares instead -- a binary search, not a chain.
   Count the compares and check they are ordered like a tree, not a list. */
int sparse(int k) {
    switch (k) {
        case 1:       return 1;
        case 1000:    return 2;
        case 50000:   return 3;
        case 999999:  return 4;
        case -7:      return 5;
    }
    return -1;
}

/* Membership test. clang often turns this into a BIT MASK: one shift and one
   test against a constant, no branching per case at all. Look for tbz/tst
   against a magic number, and work out which bits are set. */
int is_vowel(char c) {
    switch (c) {
        case 'a': case 'e': case 'i': case 'o': case 'u': return 1;
    }
    return 0;
}

/* Two ranges. Watch how a range becomes "subtract the low bound, then one
   unsigned compare against the width" -- one compare, not two. */
int is_alnum(int c) {
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')) return 1;
    return 0;
}
