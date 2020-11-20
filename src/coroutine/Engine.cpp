#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <cassert>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
	char addr = 0;
    if(&ctx != idle_ctx){
        ctx.Low = &addr;
    }
    assert (ctx.Hight - ctx.Low >= 0);
    std::size_t need_mem = ctx.Hight - ctx.Low;
    if(std::get<1>(ctx.Stack) < need_mem || std::get<1>(ctx.Stack) > 2*need_mem) {
        delete [] std::get<0>(ctx.Stack);
        std::get<1>(ctx.Stack) = need_mem;
        std::get<0>(ctx.Stack) = new char[need_mem];
    }
    memcpy(std::get<0>(ctx.Stack), ctx.Low, need_mem);
}

void Engine::Restore(context &ctx) {
	char cur_addr;
    while(&cur_addr > ctx.Low) {
        Restore(ctx);
    }
    assert(&cur_addr < ctx.Hight);
    std::size_t memory_restore = ctx.Hight - ctx.Low;
	if (&ctx != idle_ctx) {
        memcpy(ctx.Low, std::get<0>(ctx.Stack), memory_restore);
    }
    cur_routine = &ctx;
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
	sched(nullptr);
}

void Engine::sched(void *routine_) {
    auto routine = (context*)routine_;
    if(find(blocked, routine)) {
        return;
    }
	if (routine == nullptr){
        if (alive == cur_routine && alive->next != nullptr) {
            routine_ = alive->next;
        }
        routine = alive;
    }
    if(cur_routine != idle_ctx) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }
    Restore(*routine);
}

bool Engine::find(context*& head, context*& elmt)
{
	if (!head){
		return false;
	}
	if (head == elmt){
		return true;
	}
	find(head->next, elmt);
}

void Engine::remove_head(context*& head, context*& elmt) {
    if(head == elmt) {
        head = head->next;
    }
    if (elmt->prev != nullptr) {
        elmt->prev->next = elmt->next;
    }
    if (elmt->next != nullptr) {
        elmt->next->prev = elmt->prev;
    }
}

void Engine::add_head(context*& head, context*& new_head) {
    if(head == nullptr) {
        head = new_head;
        head->prev = nullptr;
        head->next = nullptr;
    } else {
        new_head->prev = nullptr;
        if(head != nullptr) {
            head->prev = new_head;
        }
        new_head->next = head;
        head = new_head;
    }
}

void Engine::block(void *cor){
    auto blocking = (context*)cor;
    if (cor == nullptr){
        blocking = cur_routine;
    }
    remove_head(alive, blocking);
    add_head(blocked, blocking);
    if(blocking == cur_routine) {
        yield();
    }
}

void Engine::unblock(void* cor){
    auto unblocking = (context*)cor;
    if (unblocking == nullptr){
        return;
    }
    remove_head(blocked, unblocking);
    add_head(alive, unblocking);
}

} // namespace Coroutine
} // namespace Afina
