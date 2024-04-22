using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows.Threading;

namespace UnrealFrontend
{
	public class WorkerThread
	{
		public delegate void VoidTask();

		private struct WorkItem
		{
			internal Delegate FuncToCall;
			internal EventWaitHandle SetOnComplete;
			internal object[] Params;
		}

		private AutoResetEvent WakeUpWorkerThread = new AutoResetEvent(false);
				
		private static void WorkerThreadMain( Object InWorkerThreadInstance )
		{
			WorkerThread This = (WorkerThread)InWorkerThreadInstance;
			This.WorkerThreadLoop();
		}

		private void WorkerThreadLoop()
		{
			while(bKeepGoing)
			{
				if (WakeUpWorkerThread.WaitOne())
				{
					while ( this.HasWork() || mPlatformRefreshRequest.HasValue )
					{
						// Notify that we are starting some work
						if (PickedUpTask != null)
						{
							PickedUpTask( mPlatformRefreshRequest.HasValue ? ETaskType.Refresh : ETaskType.Work );
						}

						WorkItem? SomeWorkItem = null;
						lock (mWorkQueue)
						{
							// Look for queued up platform refreshes
							if (mPlatformRefreshRequest.HasValue)
							{
								SomeWorkItem = mPlatformRefreshRequest;
								mPlatformRefreshRequest = null;
							}

							// If we don't have higher priority work, try to get some from the regular work queue.
							if ( !SomeWorkItem.HasValue && mWorkQueue.Count > 0)
							{
								SomeWorkItem = mWorkQueue.Dequeue();
							}
						}

						// Do the work.
						if (SomeWorkItem.HasValue)
						{
							SomeWorkItem.Value.FuncToCall.DynamicInvoke(SomeWorkItem.Value.Params);

							if (null != SomeWorkItem.Value.SetOnComplete)
							{
								SomeWorkItem.Value.SetOnComplete.Set();
							}
						}
					}

					// Notify that we finished all available work.
					if (WorkQueueEmpty != null)
					{
						WorkQueueEmpty();
					}

				}
			}
		}

		public bool HasWork()
		{
			return mWorkQueue.Count > 0;
		}

		public delegate void WorkQueueEmptyDelegate();
		public WorkQueueEmptyDelegate WorkQueueEmpty;

		public enum ETaskType
		{
			Refresh,
			Work,
		}
		public delegate void PickedUpTaskDelegate( ETaskType TypeOfTask );
		public PickedUpTaskDelegate PickedUpTask;

		public WorkerThread(String InName)
		{
			mWorkerThread.Name = InName;
			mWorkerThread.Start(this);
		}

		volatile bool bKeepGoing = true;
		private Queue<WorkItem> mWorkQueue = new Queue<WorkItem>();
		private Thread mWorkerThread = new Thread(WorkerThreadMain);
		private WorkItem? mPlatformRefreshRequest;

		public void QueueWork( Delegate InWorkFunc, EventWaitHandle CompletionEvent, object[] InWorkParams )
		{
			lock(mWorkQueue)
			{
				mWorkQueue.Enqueue(new WorkItem { FuncToCall = InWorkFunc, SetOnComplete = CompletionEvent, Params = InWorkParams });
			}
			WakeUpWorkerThread.Set();
		}

		public void QueueWork(WorkerThread.VoidTask InVoidTask, EventWaitHandle CompletionEvent)
		{
			this.QueueWork(InVoidTask, CompletionEvent, new object[0]);
		}

		public void QueueWork(WorkerThread.VoidTask InVoidTask)
		{
			this.QueueWork(InVoidTask, null, new object[0]);
		}

		public void QueuePlatformRefresh(Delegate InWorkFunc, params object[] InWorkParams)
		{
			lock (mWorkQueue)
			{
				mPlatformRefreshRequest = new WorkItem { FuncToCall = InWorkFunc, SetOnComplete = null, Params = InWorkParams };
			}
			WakeUpWorkerThread.Set();
		}

		public void QueuePlatformRefresh(WorkerThread.VoidTask InVoidTask)
		{
			this.QueuePlatformRefresh(InVoidTask, null);
		}

		public void Flush()
		{
			AutoResetEvent WakeUpFlusher = new AutoResetEvent(false);
			QueueWork(new VoidTask( delegate(){
				WakeUpFlusher.Set();
			} ));
			WakeUpFlusher.WaitOne();
		}

		public void CancelQueuedWork()
		{
			lock(mWorkQueue)
			{
				mWorkQueue.Clear();
			}
		}

		public void BeginShutdown()
		{
			CancelQueuedWork();
			this.bKeepGoing = false;
			WakeUpWorkerThread.Set();
		}

	}
}

