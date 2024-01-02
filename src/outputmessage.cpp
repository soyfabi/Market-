/**
 * The Forgotten Server - a free and open-source MMORPG server emulator
 * Copyright (C) 2019  Mark Samman <mark.samman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "otpch.h"

#include "outputmessage.h"
#include "protocol.h"
#include "lockfree.h"
#include "scheduler.h"

extern Scheduler g_scheduler;

namespace {

const uint16_t OUTPUTMESSAGE_FREE_LIST_CAPACITY = 2048;
const std::chrono::milliseconds OUTPUTMESSAGE_AUTOSEND_DELAY {10};

void sendAll(const std::vector<Protocol_ptr>& bufferedProtocols);

void scheduleSendAll(const std::vector<Protocol_ptr>& bufferedProtocols)
{
	g_scheduler.addEvent(createSchedulerTask(OUTPUTMESSAGE_AUTOSEND_DELAY.count(), [&]() { sendAll(bufferedProtocols); }));
}

void sendAll(const std::vector<Protocol_ptr>& bufferedProtocols)
{
	//dispatcher thread
	for (auto& protocol : bufferedProtocols) {
		auto& msg = protocol->getCurrentBuffer();
		if (msg) {
			protocol->send(std::move(msg));
		}
	}

	if (!bufferedProtocols.empty()) {
		scheduleSendAll(bufferedProtocols);
	}
}

}

void OutputMessagePool::addProtocolToAutosend(Protocol_ptr protocol)
{
	//dispatcher thread
	if (bufferedProtocols.empty()) {
		scheduleSendAll(bufferedProtocols);
	}
	bufferedProtocols.emplace_back(protocol);
}

void OutputMessagePool::removeProtocolFromAutosend(const Protocol_ptr& protocol)
{
	//dispatcher thread
	auto it = std::find(bufferedProtocols.begin(), bufferedProtocols.end(), protocol);
	if (it != bufferedProtocols.end()) {
		std::swap(*it, bufferedProtocols.back());
		bufferedProtocols.pop_back();
	}
}

OutputMessage_ptr OutputMessagePool::getOutputMessage()
{
	// LockfreePoolingAllocator<void,...> will leave (void* allocate) ill-formed because
	// of sizeof(T), so this guarantees that only one list will be initialized
	return std::allocate_shared<OutputMessage>(LockfreePoolingAllocator<void, OUTPUTMESSAGE_FREE_LIST_CAPACITY>());
}
