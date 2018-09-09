#include <string.h>
#include "unit.h"
#include "vessel_test.h"
#include "../src/list.h"
#include "../src/ringbuf.h"


int tests_run = 0;


/*
 * Tests the creation of a ringbuffer
 */
static char *test_ringbuf_init(void) {
    uint8_t buf[10];
    Ringbuf *r = ringbuf_init(buf, 10);
    ASSERT("[! ringbuf_init]: ringbuf not created", r != NULL);
    ringbuf_free(r);
    return 0;
}


/*
 * Tests the release of a ringbuffer
 */
static char *test_ringbuf_free(void) {
    uint8_t buf[10];
    Ringbuf *r = ringbuf_init(buf, 10);
    ringbuf_free(r);
    ASSERT("[! ringbuf_free]: ringbuf not released", r != NULL);
    return 0;
}


/*
 * Tests the full check function of the ringbuffer
 */
static char *test_ringbuf_full(void) {
    uint8_t buf[2];
    Ringbuf *r = ringbuf_init(buf, 2);
    ASSERT("[! ringbuf_full]: ringbuf_full doesn't work as expected, state ringbuffer is full while being empty", ringbuf_full(r) != 1);
    ringbuf_push(r, 'a');
    ringbuf_push(r, 'b');
    ASSERT("[! ringbuf_full]: ringbuf size %d", ringbuf_size(r));
    ASSERT("[! ringbuf_full]: ringbuf_full doesn't work as expected, state ringbuffer is not full while being full", ringbuf_full(r) == 1);
    ringbuf_free(r);
    return 0;
}


/*
 * Tests the empty check function of the ringbuffer
 */
static char *test_ringbuf_empty(void) {
    uint8_t buf[2];
    Ringbuf *r = ringbuf_init(buf, 2);
    ASSERT("[! ringbuf_empty]: ringbuf_empty doesn't work as expected, state ringbuffer is not empty while being empty", ringbuf_empty(r) == 1);
    ringbuf_push(r, 'a');
    ASSERT("[! ringbuf_empty]: ringbuf size %d", ringbuf_size(r));
    ASSERT("[! ringbuf_empty]: ringbuf_empty doesn't work as expected, state ringbuffer is empty while having an item", ringbuf_empty(r) != 1);
    ringbuf_free(r);
    return 0;
}


/*
 * Tests the capacity check function of the ringbuffer
 */
static char *test_ringbuf_capacity(void) {
    uint8_t buf[2];
    Ringbuf *r = ringbuf_init(buf, 2);
    ASSERT("[! ringbuf_capcacity]: ringbuf_capacity doesn't work as expected", ringbuf_capacity(r) == 2);
    ringbuf_free(r);
    return 0;
}


/*
 * Tests the size check function of the ringbuffer
 */
static char *test_ringbuf_size(void) {
    uint8_t buf[2];
    Ringbuf *r = ringbuf_init(buf, 2);
    ASSERT("[! ringbuf_size]: ringbuf_size doesn't work as expected", ringbuf_size(r) == 0);
    ringbuf_push(r, 'a');
    ASSERT("[! ringbuf_size]: ringbuf_size doesn't work as expected", ringbuf_size(r) == 1);
    ringbuf_free(r);
    return 0;
}


/*
 * Tests the push feature of the ringbuffer
 */
static char *test_ringbuf_push(void) {
    uint8_t buf[2];
    Ringbuf *r = ringbuf_init(buf, 2);
    ASSERT("[! ringbuf_push]: ringbuf_push doesn't work as expected", ringbuf_size(r) == 0);
    ringbuf_push(r, 'a');
    ASSERT("[! ringbuf_push]: ringbuf_push doesn't work as expected", ringbuf_size(r) == 1);
    uint8_t x;
    ringbuf_pop(r, &x);
    ASSERT("[! ringbuf_push]: ringbuf_push doesn't work as expected", x == 'a');
    ringbuf_free(r);
    return 0;
}


/*
 * Tests the pop feature of the ringbuffer
 */
static char *test_ringbuf_pop(void) {
    uint8_t buf[2];
    Ringbuf *r = ringbuf_init(buf, 2);
    ASSERT("[! ringbuf_pop]: ringbuf_pop doesn't work as expected", ringbuf_size(r) == 0);
    ringbuf_push(r, 'a');
    ringbuf_push(r, 'b');
    ASSERT("[! ringbuf_pop]: ringbuf_pop doesn't work as expected", ringbuf_size(r) == 2);
    uint8_t x, y;
    ringbuf_pop(r, &x);
    ASSERT("[! ringbuf_pop]: ringbuf_pop doesn't work as expected", x == 'a');
    ringbuf_pop(r, &y);
    ASSERT("[! ringbuf_pop]: ringbuf_pop doesn't work as expected", y == 'b');
    ringbuf_free(r);
    return 0;
}


/*
 * Tests the bulk_push feature of the ringbuffer
 */
static char *test_ringbuf_bulk_push(void) {
    uint8_t buf[3];
    Ringbuf *r = ringbuf_init(buf, 3);
    ASSERT("[! ringbuf_push]: ringbuf_push doesn't work as expected", ringbuf_size(r) == 0);
    ringbuf_bulk_push(r, (uint8_t *) "abc", 3);
    ASSERT("[! ringbuf_push]: ringbuf_push doesn't work as expected", ringbuf_size(r) == 3);
    uint8_t x;
    ringbuf_pop(r, &x);
    ASSERT("[! ringbuf_push]: ringbuf_push doesn't work as expected", x == 'a');
    ringbuf_free(r);
    return 0;
}


/*
 * Tests the bulk_pop feature of the ringbuffer
 */
static char *test_ringbuf_bulk_pop(void) {
    uint8_t buf[4];
    Ringbuf *r = ringbuf_init(buf, 4);
    ASSERT("[! ringbuf_bulk_pop]: ringbuf_bulk_pop doesn't work as expected", ringbuf_size(r) == 0);
    ringbuf_bulk_push(r, (uint8_t *) "abc", 3);
    ASSERT("[! ringbuf_bulk_pop]: ringbuf_bulk_pop doesn't work as expected", ringbuf_size(r) == 3);
    uint8_t x[4];
    ringbuf_bulk_pop(r, x, 3);
    ASSERT("[! ringbuf_bulk_pop]: ringbuf_bulk_pop doesn't work as expected", strcmp((const char *) x, "abc") == 0);
    ringbuf_free(r);
    return 0;
}


/*
 * Tests the init feature of the list
 */
static char *test_list_init(void) {
    List *l = list_init();
    ASSERT("[! list_init]: list not created", l != NULL);
    list_free(l, 0);
    return 0;
}


/*
 * Tests the free feature of the list
 */
static char *test_list_free(void) {
    List *l = list_init();
    ASSERT("[! list_free]: list not created", l != NULL);
    list_free(l, 0);
    return 0;
}


/*
 * Tests the push feature of the list
 */
static char *test_list_push(void) {
    List *l = list_init();
    char *x = "abc";
    list_push(l, x);
    ASSERT("[! list_push]: item not pushed in", l->len == 1);
    list_free(l, 0);
    return 0;
}


/*
 * Tests the push_back feature of the list
 */
static char *test_list_push_back(void) {
    List *l = list_init();
    char *x = "abc";
    list_push_back(l, x);
    ASSERT("[! list_push_back]: item not pushed in", l->len == 1);
    list_free(l, 0);
    return 0;
}


/*
 * All datastructure tests
 */
static char *ringbuf_tests() {
    RUN_TEST(test_ringbuf_init);
    RUN_TEST(test_ringbuf_free);
    RUN_TEST(test_ringbuf_full);
    RUN_TEST(test_ringbuf_empty);
    RUN_TEST(test_ringbuf_capacity);
    RUN_TEST(test_ringbuf_size);
    RUN_TEST(test_ringbuf_push);
    RUN_TEST(test_ringbuf_pop);
    RUN_TEST(test_ringbuf_bulk_push);
    RUN_TEST(test_ringbuf_bulk_pop);
    RUN_TEST(test_list_init);
    RUN_TEST(test_list_free);
    RUN_TEST(test_list_push);
    RUN_TEST(test_list_push_back);
    RUN_TEST(vessel_plain_test);
    RUN_TEST(vessel_ssl_test);
    return 0;
}


int main(int argc, char **argv) {
    char *result = ringbuf_tests();
    if (result != 0) {
        printf(" %s\n", result);
    }
    else {
        printf("\n [*] ALL TESTS PASSED\n");
    }
    printf(" [*] Tests run: %d\n\n", tests_run);

    return result != 0;
}
