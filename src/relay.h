#ifndef __RELAY_H__
#define __RELAY_H__

typedef enum relay_state {
    RELAY_ON,
    RELAY_OFF,
    RELAY_ERR
}relay_state_t;

int relay_init(void);
void relay_set(relay_state_t state);
relay_state_t relay_get(void);

#endif /* __RELAY_H__ */