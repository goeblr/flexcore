#ifndef SRC_NODES_LIST_MANIPULATION_HPP_
#define SRC_NODES_LIST_MANIPULATION_HPP_

#include <core/traits.hpp>
#include <ports/ports.hpp>
#include <nodes/node_interface.hpp>
#include <ports/fancy_ports.hpp>

// std
#include <map>

// boost
#include <boost/range.hpp>

namespace fc
{

/**
 * \brief node for splitting lists according to a predicate
 *
 * out() generates a separate output port for every predicate
 * result value.
 *
 * \tparam range_t: any type for which non member functions begin() and end() exist.
 * \tparam predicate_result_t: result type of predicate
 */
template
	<	class range_t,
		class predicate_result_t
	>
class list_splitter : public node_interface
{
public:
	typedef typename std::iterator_traits<decltype(boost::begin(range_t()))>::value_type value_t;
	typedef boost::iterator_range<typename std::vector<value_t>::iterator> out_range_t;

	list_splitter(auto pred)
		: node_interface("splitter")
		, in(this, [&](const range_t& range){ this->receive(range); } )
		, out_num_dropped(this)
		, entries()
		, predicate(pred)
	{}

	event_sink<range_t> in;
	event_source<out_range_t> out(predicate_result_t value)
	{
		auto it = entries.find(value);
		if (it == entries.end())
			it = entries.insert(std::make_pair(value, entry_t(this))).first;
		return it->second.port;
	}
	/**
	 * number of dropped elements (due to unconnected output ports)
	 * (Can be used for verification)
	 */
	state_source_with_setter<size_t> out_num_dropped;

private:
	void receive(const range_t& range)
	{
		auto begin = std::begin(range);
		auto end = std::end(range);

		// sort elements by predicate
		for (auto it = begin; it != end; ++it)
		{
			auto p = predicate(*it);
			auto entry_it = entries.find(p);
			if (entry_it != entries.end())
				entry_it->second.data.push_back(*it);
			else
				++out_num_dropped.access();
		}
		// send sorted elements
		for (auto& e : entries)
		{
			e.second.port.fire(boost::make_iterator_range( e.second.data.begin(),
														   e.second.data.end() ));
			e.second.data.clear();
		}
	}
	struct entry_t
	{
		entry_t(node_interface* p)
			: port(p)
			, data()
		{}
		event_source<out_range_t> port;
		std::vector<value_t> data;
	};

	std::map<predicate_result_t, entry_t> entries;
	std::function<predicate_result_t(value_t)> predicate;
};

/**
 * Collects list contents and store them into a buffer.
 * Sends the buffer as state when pulled.
 */
template<class range_t>
class list_collector : public node_interface
{
public:
	typedef typename std::iterator_traits<decltype(boost::begin(range_t()))>::value_type value_t;
	typedef boost::iterator_range<typename std::vector<value_t>::iterator> out_range_t;

	list_collector()
		: node_interface("list_collector")
		, in( this, [&](const range_t& range){ this->receive(range); } )
		, out( this, [&](){ return this->get_state(); } )
	{}

	event_sink<range_t> in;
	state_source_call_function<out_range_t> out;


private:
	void receive(const range_t& range)
	{
		buffer_collect.insert(buffer_collect.end(), std::begin(range), std::end(range));
	}
	out_range_t get_state()
	{
		buffer_state.clear();
		buffer_state.swap(buffer_collect);
		return boost::make_iterator_range(buffer_state.begin(), buffer_state.end());
	}
	std::vector<value_t> buffer_collect;
	std::vector<value_t> buffer_state;
};

} // namespace fc

#endif /* SRC_NODES_LIST_MANIPULATION_HPP_ */
