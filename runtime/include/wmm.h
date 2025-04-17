#pragma once

#include <atomic>
#include <map>
#include <iostream>
#include <vector>
#include <cassert>


enum class MemoryOrder {
  Relaxed,
  Acquire,
  Release,
  AcqRel,
  SeqCst
};

struct HBClock {
  HBClock(int nThreads): times(nThreads, 0) {}

  bool IsSubsetOf(const HBClock& other) {
    assert(IsSameLength(other));
    
    for (int i = 0; i < times.size(); ++i) {
      if (times[i] > other.times[i]) {
        return false;
      }
    }

    return true;
  }

  void UniteWith(const HBClock& other) {
    assert(IsSameLength(other));

    for (int i = 0; i < times.size(); ++i) {
      times[i] = std::max(times[i], other.times[i]);
    }
  }

  void Increment(int threadId) {
    assert(threadId >= 0 && threadId < times.size());
    times[threadId]++;
  }

private:
  bool IsSameLength(const HBClock& other) {
    return times.size() == other.times.size();
  }

  std::vector<int> times;
};

struct WmmUtils {
  inline static MemoryOrder moFromStd(std::memory_order mo) {
    switch (mo) {
      case std::memory_order_relaxed:
        return MemoryOrder::Relaxed;
      case std::memory_order_acquire:
        return MemoryOrder::Acquire;
      case std::memory_order_release:
        return MemoryOrder::Release;
      case std::memory_order_acq_rel:
        return MemoryOrder::AcqRel;
      case std::memory_order_seq_cst:
        return MemoryOrder::SeqCst;
      default:
        throw std::invalid_argument("Unsupported memory order");
    }
  }

  inline static std::string moToString(MemoryOrder mo) {
    switch (mo) {
      case MemoryOrder::Relaxed:
        return "Relaxed";
      case MemoryOrder::Acquire:
        return "Acquire";
      case MemoryOrder::Release:
        return "Release";
      case MemoryOrder::AcqRel:
        return "AcqRel";
      case MemoryOrder::SeqCst:
        return "SeqCst";
      default:
        throw std::invalid_argument("Unsupported memory order");
    }
  }

  // thread id to which all initalization events (constructors of atomics) will belong to
  inline static int INIT_THREAD_ID = 0;
};

namespace { // translation-unit-local details

using EdgeId = int;
using EventId = int;

enum class EdgeType {
  PO, // program order / sequenced before
  RF, // reads-from
  HB, // happens-before
  MO, // modification order
  SW, // synchronized-with
};

struct Edge {
  EdgeType type;
  EventId from;
  EventId to;
};

enum class EventType {
  DUMMY,
  READ,
  WRITE
};

struct Event {
protected:
  Event(EventId id, EventType type, int nThreads, int location, int threadId, MemoryOrder mo):
    id(id), type(type), location(location), threadId(threadId), mo(mo), clock(nThreads) {}

public:
  EventId id;
  EventType type;
  int location;
  int threadId;
  MemoryOrder mo;
  HBClock clock;
  std::vector<EdgeId> edges; // outgoing edges (e.g. `edge.to == this`)
  
  virtual ~Event() = default;
};

struct DummyEvent : Event {
  DummyEvent(EventId id, int nThreads, int threadId):
    Event(id, EventType::DUMMY, nThreads, -1 /* non-existing location */, threadId, MemoryOrder::SeqCst) {}
};

template<class T>
struct ReadEvent : Event {
  ReadEvent(EventId id, int nThreads, int location, int threadId, MemoryOrder mo):
    Event(id, EventType::READ, nThreads, location, threadId, mo), readFrom(-1) {}

  // edge.from points to write-event which we read from
  // edge.to is this event
  EdgeId readFrom;
  T value;
};

template<class T>
struct WriteEvent : Event {
  WriteEvent(EventId id, int nThreads, int location, int threadId, MemoryOrder mo, T value):
    Event(id, EventType::WRITE, nThreads, location, threadId, mo), value(std::move(value)), moBefore(-1) {}

  // edge.to points to event which goes after current in modification order
  // edge.from is this event
  EdgeId moBefore;
  T value;
};

}

class Graph {
public:
  Graph() {}
  ~Graph() { Clean(); }  

  void Reset(int nThreads) {
    Clean();
    InitThreads(nThreads);
  }

  // TODO: add `ExecutionPolicy` or other way of specifying how to create edges (Random, BoundedModelChecker, etc.)
  template<class T>
  T AddReadEvent(int location, int threadId, MemoryOrder mo) {
    EventId eventId = events.size();
    auto event = new ReadEvent<T>(eventId, nThreads, location, threadId, mo);
    
    // TODO: actually insert read event
    return T{};
  }

  template<class T>
  void AddWriteEvent(int location, int threadId, MemoryOrder mo, T value) {
    EventId eventId = events.size();
    auto event = new WriteEvent<T>(eventId, nThreads, location, threadId, mo, value);
    // TODO: actually insert write event
  }

private:
  bool IsConsistent() {
    // TODO: implement
    return true;
  }

  void Clean() {
    edges.clear();
    for (auto event : events) {
      delete event;
    }
    events.clear();
  }

  void InitThreads(int nThreads) {
    this->nThreads = nThreads;
    // insert dummy events (with all-zero hbClocks),
    // which will be the first event in each thread
    for (int t = 0; t < nThreads; ++t) {
      int eventId = events.size();
      auto dummyEvent = new DummyEvent(eventId, nThreads, t);
      events.push_back(dummyEvent);
    }
  }

  std::vector<Edge> edges;
  std::vector<Event*> events;
  int nThreads = 0;
};

class ExecutionGraph {
public:
ExecutionGraph(const ExecutionGraph&) = delete;
  ExecutionGraph& operator=(const ExecutionGraph&) = delete;
  ExecutionGraph(ExecutionGraph&&) = delete;
  ExecutionGraph& operator=(ExecutionGraph&&) = delete;

  static ExecutionGraph& getInstance() {
    static ExecutionGraph instance; // Thread-safe in C++11 and later
    return instance;
  }

  // Empties graph events and sets new number of threads. 
  void Reset(int nThreads) {
    std::cout << "Reset Graph: threads=" << nThreads << std::endl;
    this->nThreads = nThreads;
    this->nextLocationId = 0;
    graph.Reset(nThreads);
  }

  // When new location is constructed, it registers itself in the wmm-graph
  // in order to generate corresponding initialization event.
  template<class T>
  int RegisterLocation(T value) {
    int currentLocationId = nextLocationId++;
    std::cout << "Register location: loc-" << currentLocationId << ", init value=" << value << std::endl;
    graph.AddWriteEvent(currentLocationId, WmmUtils::INIT_THREAD_ID, MemoryOrder::SeqCst, value);
    return currentLocationId;
  }

  template<class T>
  T Load(int location, int threadId, MemoryOrder mo) {
    // TODO: if we now only do real atomics, then they should be stored in graph, I guess?
    std::cout << "Load: loc-" << location << ", thread="
              << threadId << ", mo=" << WmmUtils::moToString(mo) << std::endl;
    T readValue = graph.AddReadEvent<T>(location, threadId, mo);
    return readValue;
  }

  template <class T>
  void Store(int location, int threadId, MemoryOrder mo, T value) {
    std::cout << "Store: loc-" << location << ", thread=" << threadId
              << ", mo=" << WmmUtils::moToString(mo) << ", value=" << value << std::endl;
    graph.AddWriteEvent(location, threadId, mo, value);
  }

private:
  ExecutionGraph() = default;
  ~ExecutionGraph() = default;

  int nThreads = 0;
  int nextLocationId = 0;
  Graph graph;
  // TODO: here can add real atomic's name via clangpass
};