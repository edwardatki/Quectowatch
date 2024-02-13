struct notification_t {
  int id;
  char *src;
  char *title;
  char *subject;
  char *body;
  char *sender;
  char *tel;
  struct notification_t* next;
};

struct notification_t *list_root;

struct notification_t* new_notification() {
    struct notification_t *n = (notification_t*)calloc(sizeof(struct notification_t), 1);
    n->id = 0;
    n->src = NULL;
    n->title = NULL;
    n->subject = NULL;
    n->body = NULL;
    n->sender = NULL;
    n->tel = NULL;
    n->next = NULL;
    // printf("ALLOC notification\n");
    return n;
}

void delete_notification(struct notification_t* n) {
    free(n->src);
    free(n->title);
    free(n->subject);
    free(n->body);
    free(n->sender);
    free(n->tel);
    int id = n->id;
    free(n);
    // printf("FREE notification, id: %d\n", id);
}

void add_notification(struct notification_t* n) {
    if (list_root != NULL) n->next = list_root; 
    list_root = n;
    printf("added notification, id: %d\n", n->id);
}

struct notification_t* get_notification(int id) {
    if (list_root != NULL) {
        struct notification_t *current_entry = list_root;
        do {
            if (current_entry->id == id) {
                printf("get notification success, id: %d\n", id);
                return current_entry;
            }
            current_entry = current_entry->next;
        } while (current_entry != NULL);
    }

    printf("get notification failed, id: %d\n", id);
    return NULL;
}

void clear_notification(int id) {
    if (list_root != NULL) {
        struct notification_t *current_entry = list_root;
        struct notification_t *previous_entry = NULL;
        do {
            if (current_entry->id == id) {
                if (previous_entry == NULL) list_root = current_entry->next;
                else previous_entry->next = current_entry->next;
                delete_notification(current_entry);
                printf("cleared notification, id: %d\n", id);
                return;
            }
            previous_entry = current_entry;
            current_entry = current_entry->next;
        } while (current_entry != NULL);
    }

    printf("failed to clear notification, id: %d\n", id);
}

struct notification_t* next_notification(struct notification_t* n) {
    if (list_root == NULL) return NULL;
    if (n == NULL) return list_root;
    else return n->next;
}

int notification_count() {
    int count = 0;
    if (list_root != NULL) {
        count = 1;
        struct notification_t *current_entry = list_root;
        while (current_entry->next != NULL) {
            count++;
            current_entry = current_entry->next;
        }
    }
    // printf("counted %d notifications\n", count);
    return count;
}

struct notification_t* latest_notification(int id) {
    return list_root;
}