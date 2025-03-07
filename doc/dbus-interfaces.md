# Hirte interfaces

Hirte is a tool that allows you to manage a multitude of systems, each running systemd, in a single place.

The main service in Hirte is called the manager. It runs on some kind of Linux system, that Linux system itself may be
connected to Hirte, but that is not strictly necessary.

When the manager starts it loads a configuration file that describes all the systems (called nodes) that are to be
managed. Each node has a unique name that is used to reference it in the manager. Optionally the configuration also
contains authentication information for each node (using client certificates).

On each node that is under control of Hirte a service called `hirte-agent` is running, when it starts up it connects
(via D-Bus over TCP) to the manager and registers as available (optionally authenticating). It then receives requests
from the manager and reports local state changes to it.

The main way to interact with Hirte is using D-Bus. It exposes a name on the system bus called `org.containers.hirte`
that other programs can use to control the system. It is expected that high-level control planes use this API directly,
but there is also a `hirtectl` program that is useful for debugging and testing.

## Hirte public D-Bus API

The main entry point is at the `/org/containers/hirte` object path and implements the `org.containers.hirte.Manager`
interface.

Note that all properties also come with change events, so you can easily track when they change.

### interface org.containers.hirte.Manager

* Methods:

  * `ListUnits(out a(sssssssouso) units)`

    Returns an array with all currently loaded systemd units on all online nodes. This is equivalent to calling
    `Node.ListUnits()` on all the online nodes and adding the name of the node as the first element of each returned
    element struct.

  * `CreateMonitor(out o monitor)`

    Creates a new monitor object, which can be used to monitor the state of selected units on the nodes. The monitor
    object returned will automatically be closed if the calling peer disconnects from the bus.

  * `ListNodes(out a(sos) nodes)`

    Returns information (name, object_path and status) of all known nodes.

  * `GetNode(in s name, out o path)`

    Returns the object path of a node given its name.

* Signals:

  * `JobNew(u id, o job, s nodeName, s unit)`

    Emitted each time a new Hirte job is queued.

  * `JobRemoved(u id, o job, s nodeName, s unit, s result)`

    Emitted each time a new job is dequeued or the underlying systemd job finished. `result` is one of: `done`,
    `failed`, `cancelled`, `timeout`, `dependency`, `skipped`. This is either the result from systemd on the node, or
    `cancelled` if the job was cancelled in Hirte before any systemd job was started for it.

* Properties:

  * `Nodes` - `as`

    A list with the names of all configured nodes managed by Hirte. Each name listed here also has a corresponding
    object under `/org/containers/hirte/node/$name` which implements the `org.containers.hirte.Node` interface.

### interface org.containers.hirte.Monitor

Object path: `/org/containers/hirte/monitor/$id`

* Methods:

  * `Close()`

    Stop monitoring and delete the Monitor object, removing any outstanding subscriptions.

  * `Subscribe(in node s, in unit s)`

    Subscribe to changes in properties of a given unit on a given node, or all nodes if the node name is empty. This
    will emit the signal `UnitChanged` for all matching units in the system, and then again whenever one of the
    properties of the unit changes.

  * `Unsubscribe(in node s, in unit s)`

    Remove an earlier added subscription.

* Signals:

  * `UnitPropertiesChanged(s node, s unit, s interface, a{sv} props)`

    Whenever the properties changes for any of the units that are currently subscribed to changes, this signal is
    emitted. Additionally it is emitted initially once for each matched unit. This allows you to easily monitor and get
    the current state in a race-free way without missing any changes.

  * UnitNew(s node, s unit, s reason)

     Emitted when a new unit is loaded by systemd, for example when a
     service is started (reason=`real`), or if hirte learns of an
     already loaded unit (reason=`virtual`).

    The latter can happen for two reasons, either hirte already knows
    that the unit is loaded. Or, at a later time a new agent connects
    to a previously offline node and the unit was already running
    on the node.

  * UnitStateChanged(s node, s unit, s active_state, s substate, s reason)

    Emitted when the active state (and substate) of a monitored unit
    changes. Additionally, when a new subscription is added to a unit that
    is already active, a virtual event is sent. This makes it very easy
    to track the current active state of a unit.

  * UnitRemove(s node, s unit, s reason)

    Emitted when a unit is unloaded by systemd (reason=`real`), or
    when the agent disconnects and we previously reported the unit
    as loaded (reason=`virtual`).

### interface org.containers.hirte.Node

Each node object represents a configured node in the system, independent of whether that node is connected to the
manager or not, and the status can change over time.

Object path: `/org/containers/hirte/node/$name`

* Methods:

  * `StartUnit(in s name, in s mode, out o job)`

    Queues a unit activate job for the named unit on this node. The queue is per-unit name, which means there is only
    ever one active job per unit. Mode can be one of `replace` or `fail`. If there is an outstanding queued (but not
    running) job, that is replaced if mode is `replace`, or the job fails if mode is `fail`.

    The job returned is an object path for an object implementing `org.containers.hirte.Job`, and which be monitored for
    the progress of the job, or used to cancel the job. To track the result of the job, follow the `JobRemoved` signal
    on the Manager.

  * `StopUnit(in s name, in s mode, out o job)`

    `StopUnit()` is similar to `StartUnit()` but stops the specified unit rather than starting it.

  * `ReloadUnit(in  s name, in  s mode, out o job)`

  * `RestartUnit(in  s name, in  s mode, out o job)`

    `ReloadUnit()`/`RestartUnit()` is similar to `StartUnit()` but can be used to reload/restart a unit instead. See
    equivalent systemd methods for details.

  * `KillUnit(in  s name, in  s who, in  i signal)`

    Kill a unit on the node. Arguments and semantics are equivalent to the systemd `KillUnit()` method.

  * `GetUnitProperties(in s name, in s interface, out a{sv} props)`

    Returns the current properties for a named unit on the node. The returned properties are the same as you would
    get in the systemd properties apis.

  * `GetUnitProperty(in s name, in s interface, in property, out v value)`

    Get one named property, otherwise similar to GetUnitProperties

  * `SetUnitProperties(in s name, in b runtime, in a(sv) keyvalues)`

    Set named properties. If runtime is true the property changes do not persist across
    reboots.

  * `ListUnits(out a(ssssssouso) units)`

    Returns all the currently loaded systemd units on this node. The returned structure is the same as the one returned
    by the systemd `ListUnits()` call.

* Properties:

  * `Name` - `s`

    The name of the node

  * `Status` - `s`

    Status of the node, currently one of: `online`, `offline`. Emits changed when this changes.

### interface org.containers.hirte.Job

Each potentially long-running operation returns a job object, which can be used to monitor the status of the job as well
as cancelling it.

Object path: `/org/containers/hirte/job/$id`

* Methods:

  * `Cancel()`

    Cancel the job, which means either cancelling the corresponding systemd job if it was started, or directly
    cancelling or replacing the Hirte job if no systemd job was started for it yet (i.e. it is in the queue).

* Properties

  * `Id` - `u`

    An integer giving the id of the job

  * `Node` - `s`

    The name of the node the job is on

  * `Unit` - `s`

    The name of the unit the job works on

  * `JobType` - `s`

    Type of the job, either `Start` or `Stop`.

  * `State` - `s`

    The current state of the job, one of: `waiting` or `running`. Waiting is for queued jobs.

## Hirte-Agent public D-Bus API

The main entry point is at the `/org/containers/hirte` object path and implements the `org.containers.hirte.Agent`
interface.

### interface org.containers.hirte.Agent

* Methods:

  * `CreateProxy(in s service_name, in s node_name, in s unit_name)`

    Whenever a service on the agent requires a service on another node it creates a proxy service and calls this method.
    It then creates a new `org.containers.internal.Proxy` object and emits the `ProxyNew` signal on the internal bus to
    tell the manager about it. The manager will then try to arrange that the requested unit on the specified node is
    running and notifies the initial agent about the status by calling `Ready` on the internal bus.

  * `RemoveProxy(in s service_name, in s node_name, in s unit_name)`

    When a proxy is not needed anymore it is being removed on the node and a `ProxyRemoved` is emitted to notify the manager.

## Internal D-Bus APIs

The above APIs are the public facing ones that users of Hirte would use. Additionally there are additional APIs that are
used internally to synchronize between the manager and the nodes, and sometimes internally on a node. We here describe
these APIs.

### interface org.containers.hirte.internal.Manager

When a node connects to the manager it does so not via the public API, but via a direct peer-to-peer connection. On this
connection the regular Manager API is not available, instead we're using internal Manager object as the basic data.

Object path: `/org/containers/hirte/internal`

* Methods:

  * `Register(in st name)`

    Before anything else can happen the node must call this method to register with the manager, giving its unique name.
    If this succeeds, then the manager will consider the node online and start forwarding operations to it.

### interface org.containers.hirte.internal.Agent

This is the main interface that the node implements and that is used by the manager to affect change on the node.

* Methods:

  * `StartUnit(in s name, in s mode, in u id)`

  * `StopUnit(in s name, in s mode, in u id)`

  * `ReloadUnit(in s name, in s mode, in u id)`

  * `RestartUnit(in s name, in s mode, in u id)`

  * `KillUnit(in s name, in s who, in i signal)`

  * `GetUnitProperties(in name, out a{sv} props)`

  * `ListUnits(out a(ssssssouso) units);`

    These are all API mirrors of the respective method in `org.containers.hirte.Node`, and all they do is forward the
    same operation to the local systemd instance. Similarly, any changes in the systemd job will be forwarded to signals
    on the node job which will then be forwarded to the manager job and reach the user.

  * `Subscribe(in unit s)`

    Whenever some monitor object exists in the manager that matches a specific the node name and unit name, this method
    is called. This can happen either when a monitor is created or when a new node connects. Whenever *some*
    subscription is active, the node will call the systemd `Subscribed` method, and then register for `UnitNew`,
    `UnitRemoved` as well as for property change events on units. Any time a Unit changes it will emit the
    `UnitPropertyChanged` event if any of the supported properties changed since last time.

  * `Unsubscribe(in unit s)`

    Remove a subscription added via `Subscribe()`. If there are none left, call `Unsubscribe()` in the systemd API.

* Signals:

  * `JobDone(u id, s result)`

    Mirrors of the job signals in the manager and used to forward state changes from systemd to the manager.

  * `JobStateChanged(u id, s state)`

    Forwards the job state property changes from systemd to the manager.

  * `UnitPropertiesChanged(s unit, s interface, a{sv} props)`

    This is equivalent to the Monitor signal, but for this node only. The same set of properties described there is
    supported here.

  * `ProxyNew(s nodeName, s unitName, o proxy)`

    Whenever a proxy service is running on the system with the node it calls into the node service, and the node service
    creates a new `org.containers.internal.Proxy` object and emits this signal to tell the manager about it. The manager
    will notice this and try to arrange that the requested unit is running on the requested node. If the unit is already
    running, when it is started, or when the start fails, the manager will call the `Ready()` method on it.

  * `ProxyRemoved(s nodeName, s unitName)`

    This is emitted when a proxy is not needed anymore because the service requiring the proxy
    service is stopped.

  * `Heartbeat(s nodeName)`

    This is a periodic signal from the node to the manager.

### interface org.containers.hirte.internal.Proxy

The node creates one of these by request from a proxy service, it is used to synchronize the state of the remote service
with the proxy.

* Methods:

  * `Ready(in s result)`

    Called by the manager when the corresponding service is active (either was running already, or was started), or when
    it failed. result is `done` if it was already running, otherwise it is the same value as the remote node returned in
    result from its start job.
