#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <cassert>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char addr;
    if (&addr > StackBottom) {
        ctx.Hight = &addr;
    } else {
        ctx.Low = &addr;
    }

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
        return yield();
    }
    if (cur_routine) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }
    cur_routine = (context *)routine_;
    Restore(*cur_routine);
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

} // namespace Coroutine
} // namespace Afina
