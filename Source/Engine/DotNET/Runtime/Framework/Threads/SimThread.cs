using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class SimThread
    {
        public static bool IsOnIt => (SimThread_IsOnIt() != 0);

        public static Task NextFrame()
        {
            return SimThreadSynchronizationContext.Instance.NextFrame();
        }

        public static Task Delay(float seconds)
        {
            return SimThreadSynchronizationContext.Instance.Delay(seconds);
        }

        public static async Task<T> PostTask<T>(Func<T> func)
        {
            TaskCompletionSource<T> tcs = new TaskCompletionSource<T>(TaskCreationOptions.RunContinuationsAsynchronously);

            Post(() =>
            {
                try
                {
                    tcs.SetResult(func.Invoke());
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
            });

            /// From: https://learn.microsoft.com/en-us/dotnet/api/system.threading.tasks.task.configureawait
            /// When an asynchronous method awaits a Task directly,
            /// continuation usually occurs in the same thread that created the task,
            /// depending on the async context.
            /// This behavior can be costly in terms of performance and can result
            /// in a deadlock on the UI thread
            /// To avoid these problems, call Task.ConfigureAwait(false)
            return await tcs.Task.ConfigureAwait(false);
        }

        public static async Task PostTask(Action action)
        {
            TaskCompletionSource tcs = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);

            Post(() =>
            {
                try
                {
                    action.Invoke();
                    tcs.SetResult();
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
            });

            await tcs.Task.ConfigureAwait(false);
        }

        private static unsafe void Post(Action action)
        {
            SimThread_PostTask(ManagedCallback.Invoke, ManagedCallback.Create(action));
        }

        [DllImport("hyperion", EntryPoint = "SimThread_IsOnIt")]
        private static extern int SimThread_IsOnIt();

        [DllImport("hyperion", EntryPoint = "SimThread_PostTask")]
        private static extern unsafe void SimThread_PostTask(delegate* unmanaged<IntPtr, void> taskFunc, IntPtr context);
    }
}
