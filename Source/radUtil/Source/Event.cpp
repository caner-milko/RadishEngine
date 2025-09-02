#include <radUtil/Event.h>

namespace rad
{

EventSubscription::~EventSubscription()
{
	if (!Subscriber)
	{
		SubscribedEvent->RemoveSubscription(*this);
	}
}

void EventSubscriber::RemoveSubscription(std::unique_lock<std::mutex>& lock,
										 EventSubscription& subscription,
										 bool eraseSubscriptionsByEventIfEmpty)
{
	subscription.SubscribedEvent->SubscriptionRemovedBySubscriber(subscription);
	auto it = SubscriptionsByEvent.find(subscription.SubscribedEvent);
	if (it != SubscriptionsByEvent.end())
	{
		auto& subs = it->second;
		subs.erase(subscription);
		if (eraseSubscriptionsByEventIfEmpty && subs.empty())
			SubscriptionsByEvent.erase(it);
	}
	Subscriptions.erase(subscription);
}

} // namespace rad