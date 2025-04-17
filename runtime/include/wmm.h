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

  bool IsSubsetOf(const HBClock& other) const {
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
  bool IsSameLength(const HBClock& other) const {
    return times.size() == other.times.size();
  }

  std::vector<int> times;
};

struct WmmUtils {
  inline static MemoryOrder moFromStd(std::memory_order order) {
    switch (order) {
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

  inline static std::string moToString(MemoryOrder order) {
    switch (order) {
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
  SC, // seq-cst edge
  RF, // reads-from
  // TODO: do we need it? since we have hb-clocks already
  // HB, // happens-before
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
  Event(EventId id, EventType type, int nThreads, int location, int threadId, MemoryOrder order):
    id(id), type(type), location(location), threadId(threadId), order(order), clock(nThreads) {}

public:
  EventId id;
  EventType type;
  int location;
  int threadId;
  MemoryOrder order;
  HBClock clock;
  std::vector<EdgeId> edges; // outgoing edges (e.g. `edge.to == this`)
  
  virtual ~Event() = default;

  virtual void SetReadFromEvent(Event* event) {
    assert(false && "'SetReadFromEvent' can only be called on read events");
  }

  virtual void SetMoBeforeEvent(Event* event) {
    assert(false && "'SetMoBeforeEvent' can only be called on write events");
  }

  bool HappensBefore(Event* other) const {
    return clock.IsSubsetOf(other->clock);
  }

  bool IsDummy() const {
    return type == EventType::DUMMY;
  }

  bool IsWrite() const {
    return type == EventType::WRITE;
  }

  bool IsRead() const {
    return type == EventType::READ;
  }

  bool IsSeqCst() const {
    return order == MemoryOrder::SeqCst;
  }

  bool IsAcqRel() const {
    return order == MemoryOrder::AcqRel;
  }

  bool IsAcquire() const {
    return order == MemoryOrder::Acquire;
  }

  bool IsRelease() const {
    return order == MemoryOrder::Release;
  }

  bool IsRelaxed() const {
    return order == MemoryOrder::Relaxed;
  }
};

struct DummyEvent : Event {
  DummyEvent(EventId id, int nThreads, int threadId):
    Event(id, EventType::DUMMY, nThreads, -1 /* non-existing location */, threadId, MemoryOrder::SeqCst) {}
};

template<class T>
struct ReadEvent : Event {
  ReadEvent(EventId id, int nThreads, int location, int threadId, MemoryOrder order):
    Event(id, EventType::READ, nThreads, location, threadId, order), readFrom(-1) {}
  
  virtual void SetReadFromEvent(Event* event) override {
    readFrom = event->id;
  }

  // points to write-event which we read from
  EventId readFrom;
  T value;
};

template<class T>
struct WriteEvent : Event {
  WriteEvent(EventId id, int nThreads, int location, int threadId, MemoryOrder order, T value):
    Event(id, EventType::WRITE, nThreads, location, threadId, order), value(std::move(value)), moBefore(-1) {}

  void SetMoBeforeEvent(Event* event) override {
    moBefore = event->id;
  }

  // points to write event to the same location
  // which goes after current in modification order
  EventId moBefore;
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
  T AddReadEvent(int location, int threadId, MemoryOrder order) {
    EventId eventId = events.size();
    auto event = new ReadEvent<T>(eventId, nThreads, location, threadId, order);
    
    // establish po-edge
    CreatePoEdgeToEvent(event); // prevInThread --po--> event

    // TODO: implement

    return T{};
  }

  template<class T>
  void AddWriteEvent(int location, int threadId, MemoryOrder order, T value) {
    EventId eventId = events.size();
    auto event = new WriteEvent<T>(eventId, nThreads, location, threadId, order, value);

    // establish po-edge
    CreatePoEdgeToEvent(event); // prevInThread --po--> event
    
    if (order == MemoryOrder::SeqCst) {
      // establish sc-edge + Seq-Cst / MO Consistency
      CreateScEdgeToWriteEvent(event); // prevScCst --sc--> event
    }

    // Write-Write Coherence
    CreateWriteWriteCoherenceMoEdges(event);
    // TODO: Read-Write Coherence
  }

private:
  // Creates a po-edge between last event in the same thread as `event`.
  void CreatePoEdgeToEvent(Event* event) {
    int threadId = event->threadId;
    EventId eventId = event->id;

    // connect prev event in the same thread with new event via PO edge
    auto lastEventInSameThread = events[eventsPerThread[threadId].back()];
    Edge po = { EdgeType::PO, lastEventInSameThread->id, eventId };
    EdgeId poEdgeId = edges.size();
    edges.push_back(po);
    lastEventInSameThread->edges.push_back(poEdgeId);
    
    // update last event in thread
    eventsPerThread[threadId].push_back(eventId);
    
    // insert in all events vector
    events.push_back(event);

    // set correct hb-clocks for new event
    event->clock = lastEventInSameThread->clock;
    event->clock.Increment(threadId);
  }

  // Creates a sc-edge between `event` and last seq-cst write event.
  void CreateScEdgeToWriteEvent(Event* event) {
    assert(event->IsWrite() && event->IsSeqCst() && "Event must be a write with seq-cst order");
    // last seq_cst write should appear before us (location does not matter)
    if (lastSeqCstWriteEvent != -1) {
      auto lastSeqCstWrite = events[lastSeqCstWriteEvent];
      Edge sc = { EdgeType::SC, lastSeqCstWrite->id, event->id };
      EdgeId scEdgeId = edges.size();
      edges.push_back(sc);
      lastSeqCstWrite->edges.push_back(scEdgeId);
      // unite current hb-clock with last seq-cst write
      event->clock.UniteWith(lastSeqCstWrite->clock);
      
      // TODO: update mo-edges here as well but only to the same location?
      // add mo-edge if both events corrspond to the same location
      // Seq-Cst / MO Consistency
      if (lastSeqCstWrite->location == event->location) {
        Edge mo = { EdgeType::MO, lastSeqCstWrite->id, event->id };
        EdgeId moEdgeId = edges.size();
        edges.push_back(mo);
        lastSeqCstWrite->edges.push_back(moEdgeId);
      }
      
      // update last seq_cst write
      lastSeqCstWriteEvent = event->id;
    }
  }

  // establishing mo-edges between other writes that happened before `event`
  // only for the same location
  void CreateWriteWriteCoherenceMoEdges(Event* event) {
    assert(event->IsWrite());

    // iterate through each thread and find last write-event that hb `event`
    for (int t = 0; t < nThreads; ++t) {
      // iterate from most recent to earliest events in thread `t`
      const auto& threadEvents = eventsPerThread[t];
      for (auto it = threadEvents.rbegin(); it != threadEvents.rend(); ++it) {
        Event* otherEvent = events[*it];
        if (event->id == otherEvent->id) continue;
        if (
          !otherEvent->IsWrite() ||
          otherEvent->location != event->location ||
          !otherEvent->HappensBefore(event)
        ) continue;
        
        // establish mo edge
        Edge mo = { EdgeType::MO, otherEvent->id, event->id };
        EdgeId moEdgeId = edges.size();
        edges.push_back(mo);
        otherEvent->edges.push_back(moEdgeId);
        break; // no need to establish mo-edges with earlier events from this thread
      }
    }
  }

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
    eventsPerThread.clear();
    eventsPerThread.resize(nThreads);

    // insert dummy events (with all-zero hbClocks),
    // which will be the first event in each thread
    for (int t = 0; t < nThreads; ++t) {
      int eventId = events.size();
      auto dummyEvent = new DummyEvent(eventId, nThreads, t);
      events.push_back(dummyEvent);
      eventsPerThread[t].push_back(eventId);
    }
  }

  std::vector<Edge> edges;
  std::vector<Event*> events;
  std::vector<std::vector<EventId>> eventsPerThread;
  EventId lastSeqCstWriteEvent = -1;
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
  T Load(int location, int threadId, MemoryOrder order) {
    // TODO: if we now only do real atomics, then they should be stored in graph, I guess?
    std::cout << "Load: loc-" << location << ", thread="
              << threadId << ", order=" << WmmUtils::moToString(order) << std::endl;
    T readValue = graph.AddReadEvent<T>(location, threadId, order);
    return readValue;
  }

  template <class T>
  void Store(int location, int threadId, MemoryOrder order, T value) {
    std::cout << "Store: loc-" << location << ", thread=" << threadId
              << ", order=" << WmmUtils::moToString(order) << ", value=" << value << std::endl;
    graph.AddWriteEvent(location, threadId, order, value);
  }

private:
  ExecutionGraph() = default;
  ~ExecutionGraph() = default;

  int nThreads = 0;
  int nextLocationId = 0;
  Graph graph;
  // TODO: here can add real atomic's name via clangpass
};