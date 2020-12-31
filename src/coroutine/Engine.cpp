#include <afina/coroutine/Engine.h>

#include <cassert>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

Engine::~Engine() {
    if (StackBottom) {
        delete[] std::get<0>(idle_ctx->Stack);
        delete idle_ctx;
    }
    while (alive) {
        context *tmp = alive;
        delete[] std::get<0>(alive->Stack);
        delete tmp;
        alive = alive->next;
    }
    while (blocked) {
        context *tmp = blocked;
        delete[] std::get<0>(blocked->Stack);
        delete tmp;
        blocked = blocked->next;
    }
}

void Engine::Store(context &ctx) {
    char addr;
    if (&addr > StackBottom) {
        ctx.Hight = &addr;
    } else {
        ctx.Low = &addr;
    }

    std::size_t need_mem = ctx.Hight - ctx.Low;
    if (std::get<1>(ctx.Stack) < need_mem || std::get<1>(ctx.Stack) > 2 * need_mem) {
        delete[] std::get<0>(ctx.Stack);
        std::get<1>(ctx.Stack) = need_mem;
        std::get<0>(ctx.Stack) = new char[need_mem];
    }
    memcpy(std::get<0>(ctx.Stack), ctx.Low, need_mem);
}

void Engine::Restore(context &ctx) {
    char cur_addr;
    while (&cur_addr >= ctx.Low && &cur_addr <= ctx.Hight) {
        Restore(ctx);
    }
    std::size_t memory_restore = ctx.Hight - ctx.Low;
    memcpy(ctx.Low, std::get<0>(ctx.Stack), memory_restore);
    cur_routine = &ctx;
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    auto it = alive;
    if (it && it == cur_routine) {
        it = it->next;
    }
    if (it) {
        sched(it);
    }
}

void Engine::sched(void *routine_) {
    if (routine_ == nullptr || routine_ == cur_routine) {
        yield();
        return;
    }
    if (cur_routine) {
        if (cur_routine != idle_ctx) {
            if (setjmp(cur_routine->Environment) > 0) {
                return;
            }
        }
        Store(*cur_routine);
    }
    cur_routine = (context *)routine_;
    Restore(*cur_routine);
}

bool Engine::find(context *&head, context *&elmt) {
    if (!head) {
        return false;
    }
    if (head == elmt) {
        return true;
    }
    find(head->next, elmt);
}

void Engine::remove_head(context *&head, context *&elmt) {
    if (head == elmt) {
        head = head->next;
    }
    if (elmt->prev != nullptr) {
        elmt->prev->next = elmt->next;
    }
    if (elmt->next != nullptr) {
        elmt->next->prev = elmt->prev;
    }
}

void Engine::add_head(context *&head, context *&new_head) {
    if (head == nullptr) {
        head = new_head;
        head->prev = nullptr;
        head->next = nullptr;
    } else {
        new_head->prev = nullptr;
        if (head != nullptr) {
            head->prev = new_head;
        }
        new_head->next = head;
        head = new_head;
    }
}

void Engine::block(void *cor) {
    auto blocking = (context *)cor;
    if (cor == nullptr) {
        blocking = cur_routine;
    }
    remove_head(alive, blocking);
    add_head(blocked, blocking);
    if (blocking == cur_routine) {
        Restore(*idle_ctx);
    }
}

void Engine::unblock(void *cor) {
    auto unblocking = (context *)cor;
    if (unblocking == nullptr) {
        return;
    }
    remove_head(blocked, unblocking);
    add_head(alive, unblocking);
}

} // namespace Coroutine
} // namespace Afina
