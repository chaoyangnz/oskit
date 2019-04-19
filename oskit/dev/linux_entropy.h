
/* entropy(<name>, <description>, <vendor>, <author>, <filename>, <initfxn>)
*/
entropy(random, "Blocking (good entropy)", "Linux 2.2.12", "Theodore Ts-o", "random", rand_initialize)

/* You can get multiple entropy pools by including more entropy lines.
   This doesn't seem to make much sense though.
*/
    
/* End of linux_entropy.h */
