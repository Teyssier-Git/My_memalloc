#include "mem.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

// constante définie dans gcc seulement
#ifdef __BIGGEST_ALIGNMENT__
#define ALIGNMENT __BIGGEST_ALIGNMENT__
#else
#define ALIGNMENT 16
#endif

/* La seule variable globale autorisée
 * On trouve à cette adresse la taille de la zone mémoire
 */
static void* memory_addr;

static inline void *get_system_memory_adr() {
	return memory_addr;
}

static inline size_t get_system_memory_size() {
	return *(size_t*)memory_addr;
}

struct fb {
	size_t size;
	struct fb* next;
	/* ... */
};

struct variableGlobal {
	size_t size;
	struct fb* start;
};

void mem_init(void* mem, size_t taille)
{
    memory_addr = mem;
	*(size_t*)memory_addr = taille;
	assert(mem == get_system_memory_adr());
	assert(taille == get_system_memory_size());

	struct variableGlobal *vG = (struct variableGlobal *)(memory_addr);
	vG->size = taille;
	vG->start = (struct fb*)(mem+sizeof(struct variableGlobal));
	vG->start->size = taille - sizeof(struct variableGlobal);
	vG->start->next = NULL;
	mem_fit(mem_fit_worst);
}

void mem_show(void (*print)(void *, size_t, int)) {
	void *current = memory_addr + sizeof(struct variableGlobal);
	struct variableGlobal *vG = (struct variableGlobal *)memory_addr;
	struct fb *pfb = vG->start;
	while (current - memory_addr < vG->size) {
		if (current == pfb) {
			print(pfb,pfb->size,1);
			pfb = pfb->next;
		} else {
			print(current + sizeof(size_t),*(size_t *)current,0);
		}
		current = current + *(size_t *)current;
	}
}

static mem_fit_function_t *mem_fit_fn;
void mem_fit(mem_fit_function_t *f) {
	mem_fit_fn=f;
}

void *mem_alloc(size_t taille) {
	int reste = taille%sizeof(void *);
	if (reste != 0) {
		taille += reste;
	}
	struct variableGlobal *vG =(struct variableGlobal *)memory_addr;
	struct fb *fb=mem_fit_fn(vG->start,taille + sizeof(size_t));
	if (fb == NULL) {
		return NULL;
	}
	void *dispo;
	if (taille >= fb->size) {
		dispo = (void *)fb;
		if (vG->start == fb) {
			vG->start = vG->start->next;
		} else {
			struct fb *cur = vG->start;
			while (cur->next != fb) {
				cur = cur->next;
			}
			cur->next = cur->next->next;
		}
	} else {
		dispo = (void *)fb + fb->size - taille - sizeof(size_t);
		fb->size -= taille + sizeof(size_t);
	}
	*(size_t *)(dispo) = taille + sizeof(size_t);
	return (dispo + sizeof(size_t));
}


void mem_free(void* mem) {
	size_t size = mem_get_size(mem);
	//printf("%ld\n",size);
	mem = mem - sizeof(size_t);
	struct variableGlobal *vG = (struct variableGlobal *)memory_addr;
	struct fb *fb = vG->start;
	struct fb *prev;
	while (fb != NULL && (void *)fb < mem) {
		prev = fb;
		fb = fb->next;
	}
	//printf("%p\n%p\n%p\n",(void *)prev+prev->size,mem,(void *)fb);
	if ((void *)prev + prev->size == mem) {
		prev->size = prev->size + size;
		if ((void *)fb == mem + size) {
			prev->size += fb->size;
			prev->next = fb->next;
		}
	} else {
		if ((void *)fb == mem + size) {
			size += fb->size;
			struct fb *n = fb->next;
			fb = (struct fb *)mem;
			prev->next = fb;
			fb->size = size;
			fb->next = n;
		} else {
			struct fb *new = (struct fb*)mem;
			prev->next = new;
			new->size = size;
			new->next = fb;
		}
	}


	// parcourir jusqua ce que l'adresse des zones libres soit superieur et mettre le truc apres.
}


struct fb* mem_fit_first(struct fb *list, size_t size) {
	while (list != NULL && (list->size < size)) {
		list = list->next;
	}
	return list;
}

/* Fonction à faire dans un second temps
 * - utilisée par realloc() dans malloc_stub.c
 * - nécessaire pour remplacer l'allocateur de la libc
 * - donc nécessaire pour 'make test_ls'
 * Lire malloc_stub.c pour comprendre son utilisation
 * (ou en discuter avec l'enseignant)
 */
size_t mem_get_size(void *zone) {
	/* zone est une adresse qui a été retournée par mem_alloc() */

	/* la valeur retournée doit être la taille maximale que
	 * l'utilisateur peut utiliser dans cette zone */
	return *(size_t*)(zone-sizeof(size_t));
}

/* Fonctions facultatives
 * autres stratégies d'allocation
 */
struct fb* mem_fit_best(struct fb *list, size_t size) {
	list = mem_fit_first(list,size);
	//printf("%p\n", list);
	if (list != NULL) {
		size_t ecartmin = list->size - size;
		struct fb *current = list->next;
		while (current != NULL) {
			if (current->size >= size && current->size-size < ecartmin) {
				//printf("Yo!\n");
				ecartmin = current->size - size;
				list = current;
			}
			current = current->next;
		}
	}
	return list;
}

struct fb* mem_fit_worst(struct fb *list, size_t size) {
	list = mem_fit_first(list,size);
	//printf("%p\n", list);
	if (list != NULL) {
		size_t ecartmax = list->size - size;
		struct fb *current = list->next;
		while (current != NULL) {
			if (current->size >= size && current->size-size > ecartmax) {
				//printf("Yo!\n");
				ecartmax = current->size - size;
				list = current;
			}
			current = current->next;
		}
	}
	return list;
}
