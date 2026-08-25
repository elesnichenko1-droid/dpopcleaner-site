namespace DPop.Common.Restore
{
    public sealed class RestoreResult
    {
        private RestoreResult(bool success, string code, string message)
        {
            Success = success;
            Code = code;
            Message = message;
        }

        public bool Success { get; }
        public string Code { get; }
        public string Message { get; }

        public static RestoreResult Ok(string code = "restore.success", string message = null)
        {
            return new RestoreResult(true, code, message ?? string.Empty);
        }

        public static RestoreResult Fail(string code, string message = null)
        {
            return new RestoreResult(false, code, message ?? string.Empty);
        }
    }
}
