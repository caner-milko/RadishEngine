#pragma once

#include <radUtil/Common.h>

namespace rad
{

struct EventBase;
struct EventSubscriber;

struct EventSubscription
{
	EventSubscription(EventBase& event, EventSubscriber* subscriber) : SubscribedEvent(event), Subscriber(subscriber) {}
	EventSubscription(const EventSubscription&) = delete;
	EventSubscription& operator=(const EventSubscription&) = delete;
	EventSubscription(EventSubscription&&) = delete;
	EventSubscription& operator=(EventSubscription&&) = delete;
	~EventSubscription();

	Ref<EventBase> SubscribedEvent;
	EventSubscriber* Subscriber;
};

struct EventSubscriber
{
	virtual ~EventSubscriber() { RemoveAllSubscriptions(); }
	EventSubscriber() = default;
	EventSubscriber(const EventSubscriber&) = delete;
	EventSubscriber& operator=(const EventSubscriber&) = delete;
	EventSubscriber(EventSubscriber&&) = default;
	EventSubscriber& operator=(EventSubscriber&&) = default;

	void RemoveSubscriptions(EventBase& event)
	{
		std::unique_lock lock(SubscriptionsMutex);
		auto it = SubscriptionsByEvent.find(Ref<EventBase>(event));
		if (it == SubscriptionsByEvent.end())
			return;
		auto& subs = it->second;
		while (!subs.empty())
			RemoveSubscription(lock, *subs.begin(), false);
		SubscriptionsByEvent.erase(it);
	}

	void RemoveSubscription(EventSubscription& subscription)
	{
		std::unique_lock lock(SubscriptionsMutex);
		RemoveSubscription(lock, subscription);
	}

	void RemoveAllSubscriptions()
	{
		std::unique_lock lock(SubscriptionsMutex);
		for (auto& [event, subs] : SubscriptionsByEvent)
		{
			while (!subs.empty())
				RemoveSubscription(lock, *subs.begin(), false);
		}
		SubscriptionsByEvent.clear();
		Subscriptions.clear();
		assert(Subscriptions.empty() && SubscriptionsByEvent.empty());
	}

	friend EventBase;

protected:
	std::mutex SubscriptionsMutex;

private:
	void RemoveSubscription(std::unique_lock<std::mutex>& lock,
							EventSubscription& subscription,
							bool eraseSubscriptionsByEventIfEmpty = true);

	void SubscriptionRemovedByEvent(EventSubscription& subscription)
	{
		std::unique_lock lock(SubscriptionsMutex);
		auto it = SubscriptionsByEvent.find(subscription.SubscribedEvent);
		if (it != SubscriptionsByEvent.end())
		{
			auto& subs = it->second;
			subs.erase(subscription);
			if (subs.empty())
				SubscriptionsByEvent.erase(it);
		}
		Subscriptions.erase(subscription);
	}
	EventSubscription& NewSubscription(EventBase& event)
	{
		std::unique_lock lock(SubscriptionsMutex);
		auto subscription = std::make_unique<EventSubscription>(event, this);
		auto& subRef = *subscription;
		Subscriptions[subRef] = std::move(subscription);
		SubscriptionsByEvent[Ref<EventBase>(event)].insert(subRef);
		return subRef;
	}
	std::unordered_map<Ref<EventBase>, std::unordered_set<Ref<EventSubscription>>> SubscriptionsByEvent;
	// This is a map of reference of the unique ptr to unique ptr, so that we can use the reference as a key
	std::unordered_map<Ref<EventSubscription>, std::unique_ptr<EventSubscription>> Subscriptions;
};

struct EventBase
{
	struct InvokeContext
	{
		virtual ~InvokeContext() = default;
		InvokeContext() = default;
		InvokeContext(const InvokeContext&) = delete;
		InvokeContext& operator=(const InvokeContext&) = delete;
		InvokeContext(InvokeContext&&) = default;
		InvokeContext& operator=(InvokeContext&&) = default;
	};
	virtual ~EventBase() { ClearSubscriptions(); }
	void RemoveSubscription(EventSubscription& subscription)
	{
		std::unique_lock lock(SubscriptionsMutex);
		RemoveSubscription(lock, subscription);
	}

	void ClearSubscriptions()
	{
		std::unique_lock lock(SubscriptionsMutex);
		while (!Subscriptions.empty())
		{
			RemoveSubscription(lock, *Subscriptions.begin()->first);
		}
		assert(Subscriptions.empty());
	}

protected:
	EventSubscription& AddSubscriptionWithSubscriber(std::unique_ptr<InvokeContext>&& context,
													 EventSubscriber& subscriber)
	{
		std::unique_lock lock(SubscriptionsMutex);
		EventSubscription& subscription = subscriber.NewSubscription(*this);
		Subscriptions[subscription] = std::move(context);
		return subscription;
	}
	std::unique_ptr<EventSubscription> AddAndReturnSubscription(std::unique_ptr<InvokeContext>&& context)
	{
		std::unique_lock lock(SubscriptionsMutex);
		auto subscription = std::make_unique<EventSubscription>(*this, nullptr);
		Subscriptions[*subscription] = std::move(context);
		return subscription;
	}

	void RemoveSubscription(std::unique_lock<std::mutex>& lock, EventSubscription& subscription)
	{
		auto it = Subscriptions.find(subscription);
		if (it != Subscriptions.end())
		{
			Subscriptions.erase(it);
			if (subscription.Subscriber)
				subscription.Subscriber->SubscriptionRemovedByEvent(subscription);
		}
	}

	std::mutex mutable SubscriptionsMutex;

	void InvokeForAll(std::function<void(InvokeContext&)> const& callback) const
	{
		std::unique_lock lock(SubscriptionsMutex);
		for (auto& [subscription, context] : Subscriptions)
			callback(*context);
	}

private:
	friend EventSubscriber;
	void SubscriptionRemovedBySubscriber(EventSubscription& subscription)
	{
		std::unique_lock lock(SubscriptionsMutex);
		Subscriptions.erase(subscription);
	}
	struct Subscription
	{
		std::unique_ptr<InvokeContext> Context;
		Ref<EventSubscription> Subscription;
	};
	std::unordered_map<Ref<EventSubscription>, std::unique_ptr<InvokeContext>> Subscriptions;
};

template <typename... Args>
struct Event : EventBase
{
	using FuncType = std::function<void(Args...)>;
	struct FuncInvokeContext : InvokeContext
	{
		FuncInvokeContext(FuncType func) : Func(std::move(func)) {}
		FuncType Func;
	};
	EventSubscription& Add(EventSubscriber& subscriber, FuncType func)
	{
		return AddSubscriptionWithSubscriber(std::make_unique<FuncInvokeContext>(std::move(func)), subscriber);
	}
	template <auto Method, typename Class>
	EventSubscription& Add(Class& object)
	{
		return Add(object, [ref = Ref<Class>(object)](const Args&... args) {
			((*ref).*Method)(args...);
		});
	}

	std::unique_ptr<EventSubscription> Add(std::function<void(const Args&...)> func)
	{
		auto context = std::make_unique<FuncInvokeContext>(std::move(func));
		return AddAndReturnSubscription(std::move(context));
	}

	void Broadcast(Args... args) const
	{
		InvokeForAll([&](InvokeContext& ctx) {
			static_cast<FuncInvokeContext&>(ctx).Func(args...);
		});
	}
};

} // namespace rad