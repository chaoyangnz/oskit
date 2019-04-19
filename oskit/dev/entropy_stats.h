#ifndef _OSKIT_DEV_ENTROPY_STATS_H_
#define _OSKIT_DEV_ENTROPY_STATS_H_

struct oskit_entropy_stats {
    
    /* Number of calls to entropy_getstats. */
    unsigned int n_calls;

    /* Estimate of the amount entropy in the pool. */
    unsigned int entropy_count;
};
typedef struct oskit_entropy_stats oskit_entropy_stats_t;

#endif /* _OSKIT_DEV_ENTROPY_STATS_H_ */
