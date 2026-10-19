using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Runtime.ExceptionServices;
using System.Threading;
using System.Threading.Tasks;

namespace Hyperion
{
    public sealed class SimThreadSynchronizationContext : SynchronizationContext
    {
        private struct PendingDelay
        {
            public float remaining;
            public TaskCompletionSource completion;
        }

        public static SimThreadSynchronizationContext Instance { get; } = new SimThreadSynchronizationContext();

        private readonly ConcurrentQueue<(SendOrPostCallback callback, object? state)> _posted = new ConcurrentQueue<(SendOrPostCallback, object?)>();
        private readonly object _waitersLock = new object();
        private List<TaskCompletionSource> _nextFrame = new List<TaskCompletionSource>();
        private readonly List<PendingDelay> _delays = new List<PendingDelay>();
        private int _threadId = -1;

        private SimThreadSynchronizationContext()
        {
        }

        public bool IsOnSimThread => Environment.CurrentManagedThreadId == _threadId;

        public override void Post(SendOrPostCallback callback, object? state)
        {
            _posted.Enqueue((callback, state));
        }

        public override void Send(SendOrPostCallback callback, object? state)
        {
            if (IsOnSimThread)
            {
                callback(state);

                return;
            }

            using ManualResetEventSlim done = new ManualResetEventSlim();
            ExceptionDispatchInfo? error = null;

            Post(_ =>
            {
                try
                {
                    callback(state);
                }
                catch (Exception ex)
                {
                    error = ExceptionDispatchInfo.Capture(ex);
                }
                finally
                {
                    done.Set();
                }
            }, null);

            done.Wait();
            error?.Throw();
        }

        public override SynchronizationContext CreateCopy()
        {
            return this;
        }

        public Task NextFrame()
        {
            TaskCompletionSource completion = new TaskCompletionSource();

            lock (_waitersLock)
            {
                _nextFrame.Add(completion);
            }

            return completion.Task;
        }

        public Task Delay(float seconds)
        {
            TaskCompletionSource completion = new TaskCompletionSource();

            lock (_waitersLock)
            {
                _delays.Add(new PendingDelay { remaining = seconds, completion = completion });
            }

            return completion.Task;
        }

        public void Tick(float deltaTime)
        {
            if (_threadId != Environment.CurrentManagedThreadId)
            {
                _threadId = Environment.CurrentManagedThreadId;
            }

            if (Current != this)
            {
                SetSynchronizationContext(this);
            }

            List<TaskCompletionSource> nextFrame;
            List<TaskCompletionSource>? delaysDone = null;

            lock (_waitersLock)
            {
                nextFrame = _nextFrame;
                _nextFrame = new List<TaskCompletionSource>();

                for (int i = _delays.Count - 1; i >= 0; i--)
                {
                    PendingDelay delay = _delays[i];
                    delay.remaining -= deltaTime;

                    if (delay.remaining > 0.0f)
                    {
                        _delays[i] = delay;

                        continue;
                    }

                    (delaysDone ??= new List<TaskCompletionSource>()).Add(delay.completion);
                    _delays.RemoveAt(i);
                }
            }

            foreach (TaskCompletionSource completion in nextFrame)
            {
                completion.SetResult();
            }

            if (delaysDone != null)
            {
                for (int i = delaysDone.Count - 1; i >= 0; i--)
                {
                    delaysDone[i].SetResult();
                }
            }

            for (int count = _posted.Count; count > 0 && _posted.TryDequeue(out (SendOrPostCallback callback, object? state) posted); count--)
            {
                try
                {
                    posted.callback(posted.state);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, "Exception thrown by code resumed on the sim thread: {0}", ex);
                }
            }
        }
    }
}
