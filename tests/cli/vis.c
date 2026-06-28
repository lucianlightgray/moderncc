/* hidden_att: explicit hidden visibility. shown_one: explicit default.
   plain_one: undecorated (becomes hidden only under -fvisibility=hidden). */
__attribute__((visibility("hidden"))) int hidden_att(void){ return 1; }
__attribute__((visibility("default"))) int shown_one(void){ return 2; }
int plain_one(void){ return 3; }
