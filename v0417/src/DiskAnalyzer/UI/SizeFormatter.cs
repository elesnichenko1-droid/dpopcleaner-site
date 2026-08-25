using System;
using System.Globalization;
using DPop.DiskAnalyzer.Model;

namespace DPop.DiskAnalyzer.UI
{
    public static class SizeFormatter
    {
        private static readonly string[] Units = { "Б", "КБ", "МБ", "ГБ", "ТБ", "ПБ" };

        public static string LogicalText(DiskNode node)
        {
            if (node == null) throw new ArgumentNullException(nameof(node));
            return BytesText(node.LogicalBytes);
        }

        public static string AllocatedText(DiskNode node)
        {
            if (node == null) throw new ArgumentNullException(nameof(node));
            if (!node.AllocatedComplete || !node.AllocatedBytes.HasValue)
                return "—";
            return BytesText(node.AllocatedBytes.Value);
        }

        public static string BytesText(long bytes)
        {
            if (bytes < 0) bytes = 0;
            double value = bytes;
            var unit = 0;
            while (value >= 1024.0 && unit < Units.Length - 1)
            {
                value /= 1024.0;
                unit++;
            }

            var format = unit == 0 ? "0" : "0.##";
            return value.ToString(format, CultureInfo.CurrentCulture) + " " + Units[unit];
        }
    }
}
