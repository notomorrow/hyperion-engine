using System;
using System.Diagnostics;

namespace Hyperion
{
    [ClassBinding(IsDynamic = true)]
    public class ExternalEditorTask : TickableEditorTask
    {
        private readonly object _lock = new object();
        private readonly string _title;
        private string _step;
        private Stopwatch _stepTimer = Stopwatch.StartNew();
        private volatile bool _isCompleted;

        public Action? OnCancelAction { get; set; }

        public ExternalEditorTask(string title, string step)
        {
            _title = title;
            _step = step;
        }

        public override void Start()
        {
            Title = _title;
            Description = _step;

            base.Start();
        }

        public void SetStep(string step, float progress)
        {
            lock (_lock)
            {
                _step = step;
                _stepTimer = Stopwatch.StartNew();
            }

            this.SetProgress(progress);

            Description = step;
        }

        public void Complete()
        {
            _isCompleted = true;
        }

        public override void Cancel()
        {
            OnCancelAction?.Invoke();

            base.Cancel();
        }

        public override bool IsCompleted()
        {
            return _isCompleted;
        }

        public override void Tick()
        {
            string description;

            lock (_lock)
            {
                description = $"{_step} ({_stepTimer.Elapsed:m\\:ss})";
            }

            Description = description;
        }
    }
}
