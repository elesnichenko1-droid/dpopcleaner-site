using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;

namespace DPop.Common.History
{
    public sealed class HistoryStore
    {
        private readonly string _historyDirectory;
        private readonly JavaScriptSerializer _serializer = new JavaScriptSerializer();

        public HistoryStore(string historyDirectory)
        {
            if (string.IsNullOrWhiteSpace(historyDirectory))
                throw new ArgumentException("History directory is required.", nameof(historyDirectory));

            _historyDirectory = Path.GetFullPath(historyDirectory);
            Directory.CreateDirectory(_historyDirectory);
        }

        public string HistoryDirectory => _historyDirectory;

        public void Append(HistoryRecord record)
        {
            if (record == null) throw new ArgumentNullException(nameof(record));
            if (record.Id == Guid.Empty) throw new InvalidDataException("History record Id must not be empty.");
            if (record.TimestampUtc == default(DateTime)) throw new InvalidDataException("History record timestamp is required.");
            if (record.TimestampUtc.Kind != DateTimeKind.Utc)
                throw new InvalidDataException("History record timestamp must be UTC.");
            if (string.IsNullOrWhiteSpace(record.OperationId))
                throw new InvalidDataException("History record operation id is required.");

            var idSuffix = "-" + record.Id.ToString("N") + ".json";
            if (Directory.EnumerateFiles(_historyDirectory, "*.json", SearchOption.TopDirectoryOnly)
                .Any(path => Path.GetFileName(path).EndsWith(idSuffix, StringComparison.OrdinalIgnoreCase)))
            {
                throw new InvalidOperationException("History record already exists: " + record.Id.ToString("D"));
            }

            var fileName = record.TimestampUtc.ToString("yyyyMMdd-HHmmssfff") + idSuffix;
            var finalPath = Path.Combine(_historyDirectory, fileName);
            var temporaryPath = finalPath + ".tmp";
            if (File.Exists(finalPath) || File.Exists(temporaryPath))
                throw new InvalidOperationException("History record path already exists.");

            var json = _serializer.Serialize(record);
            try
            {
                using (var stream = new FileStream(temporaryPath, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                using (var writer = new StreamWriter(stream))
                {
                    writer.Write(json);
                    writer.Flush();
                    stream.Flush(true);
                }
                File.Move(temporaryPath, finalPath);
            }
            finally
            {
                if (File.Exists(temporaryPath))
                    File.Delete(temporaryPath);
            }
        }

        public IReadOnlyList<HistoryRecord> ReadAll()
        {
            var records = new List<HistoryRecord>();
            foreach (var path in Directory.EnumerateFiles(_historyDirectory, "*.json", SearchOption.TopDirectoryOnly))
            {
                HistoryRecord record;
                try
                {
                    record = _serializer.Deserialize<HistoryRecord>(File.ReadAllText(path));
                }
                catch (Exception ex)
                {
                    throw new InvalidDataException("History record is corrupt: " + Path.GetFileName(path), ex);
                }

                if (record == null || record.Id == Guid.Empty)
                    throw new InvalidDataException("History record is invalid: " + Path.GetFileName(path));
                records.Add(record);
            }

            return records
                .OrderByDescending(record => record.TimestampUtc)
                .ThenByDescending(record => record.Id)
                .ToList()
                .AsReadOnly();
        }

        public HistoryRecord Find(Guid id)
        {
            return ReadAll().FirstOrDefault(record => record.Id == id);
        }
    }
}
