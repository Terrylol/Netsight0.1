#include "netsight.hh"

/*
 * insert p after loc
 * if loc is NULL, insert at the head of the list
 * */
void 
PostcardList::insert(PostcardNode *p, PostcardNode *loc)
{
    if(loc == NULL) {
        p->next = head;
        if(p->next != NULL) {
            p->next->prev = p;
        }
        head = p;
        if(tail == NULL)
            tail = p;
    }
    else if(loc == tail) {
        p->prev = loc;
        loc->next = p;
        tail = p;
    }
    else {
        p->next = loc->next;
        p->prev = loc;
        p->next->prev = p;
        loc->next = p;
    }
    length++;
}

PostcardNode*
PostcardList::remove(PostcardNode *p)
{
    if(p->prev != NULL) {
        p->prev->next = p->next;
    }
    else {
        head = p->next;
    }

    if(p->next != NULL) {
        p->next->prev = p->prev;
    }
    else {
        tail = p->prev;
    }
    p->next = p->prev = NULL;
    length--;
    return p;
}

void 
PostcardList::clear()
{
    PostcardNode *p = head;
    while(p != NULL) {
        PostcardNode *next = p->next;
        free(p->pkt);
        free(p);
        p = next;
    }
    head = tail = NULL;
    length = 0;
}

void 
PostcardList::print()
{
    PostcardNode *pn = head;
    while(pn) {
        pn->print();
        pn = pn->next;
        if(pn)
            printf(" -> ");
    }
    printf("\n");
}
