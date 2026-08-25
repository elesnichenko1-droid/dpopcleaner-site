using System;
using System.Collections.Generic;
using System.IO;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace DPop.Common.Restore
{
    public sealed class HkcuRegistryValueProvider
    {
        private readonly JavaScriptSerializer _json = new JavaScriptSerializer();

        public static string CreateTarget(string subKey, string valueName, RegistryValueKind valueKind)
        {
            if (string.IsNullOrWhiteSpace(subKey))
                throw new ArgumentException("A registry subkey is required.", nameof(subKey));

            var serializer = new JavaScriptSerializer();
            return serializer.Serialize(new Dictionary<string, object>
            {
                ["hive"] = "HKCU",
                ["subKey"] = subKey,
                ["valueName"] = valueName ?? string.Empty,
                ["valueKind"] = valueKind.ToString(),
            });
        }

        public string Capture(string targetJson)
        {
            var target = ParseTarget(targetJson);
            using (var key = Registry.CurrentUser.OpenSubKey(target.SubKey, false))
            {
                var exists = key != null && Array.Exists(
                    key.GetValueNames(),
                    name => string.Equals(name, target.ValueName, StringComparison.Ordinal));

                if (!exists)
                {
                    return _json.Serialize(new Dictionary<string, object>
                    {
                        ["exists"] = false,
                    });
                }

                var kind = key.GetValueKind(target.ValueName);
                var value = key.GetValue(target.ValueName, null, RegistryValueOptions.DoNotExpandEnvironmentNames);
                return _json.Serialize(new Dictionary<string, object>
                {
                    ["exists"] = true,
                    ["kind"] = kind.ToString(),
                    ["value"] = EncodeValue(kind, value),
                });
            }
        }

        public void Restore(string targetJson, string stateJson)
        {
            var target = ParseTarget(targetJson);
            if (string.IsNullOrWhiteSpace(stateJson))
                throw new InvalidDataException("Registry restore state is empty.");

            Dictionary<string, object> state;
            try
            {
                state = _json.Deserialize<Dictionary<string, object>>(stateJson);
            }
            catch (Exception ex)
            {
                throw new InvalidDataException("Registry restore state is invalid.", ex);
            }

            object existsValue;
            if (state == null || !state.TryGetValue("exists", out existsValue) || !(existsValue is bool))
                throw new InvalidDataException("Registry restore state has no valid exists flag.");

            if (!(bool)existsValue)
            {
                using (var key = Registry.CurrentUser.OpenSubKey(target.SubKey, true))
                {
                    key?.DeleteValue(target.ValueName, false);
                }
                return;
            }

            object kindValue;
            object encodedValue;
            if (!state.TryGetValue("kind", out kindValue) || !(kindValue is string) ||
                !state.TryGetValue("value", out encodedValue))
            {
                throw new InvalidDataException("Registry restore state is incomplete.");
            }

            RegistryValueKind kind;
            if (!Enum.TryParse((string)kindValue, true, out kind))
                throw new InvalidDataException("Registry restore state has an invalid value kind.");

            var value = DecodeValue(kind, encodedValue);
            using (var key = Registry.CurrentUser.CreateSubKey(target.SubKey))
            {
                if (key == null) throw new IOException("Unable to open HKCU restore target.");
                key.SetValue(target.ValueName, value, kind);
            }
        }

        private RegistryTarget ParseTarget(string targetJson)
        {
            if (string.IsNullOrWhiteSpace(targetJson))
                throw new InvalidDataException("Registry restore target is empty.");

            Dictionary<string, object> target;
            try
            {
                target = _json.Deserialize<Dictionary<string, object>>(targetJson);
            }
            catch (Exception ex)
            {
                throw new InvalidDataException("Registry restore target is invalid.", ex);
            }

            var hive = GetString(target, "hive");
            var subKey = GetString(target, "subKey");
            var valueName = GetString(target, "valueName", allowEmpty: true);
            var valueKind = GetString(target, "valueKind");

            if (!string.Equals(hive, "HKCU", StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Only HKCU registry targets are allowed.");
            if (string.IsNullOrWhiteSpace(subKey))
                throw new InvalidDataException("Registry subkey is empty.");

            RegistryValueKind parsedKind;
            if (!Enum.TryParse(valueKind, true, out parsedKind))
                throw new InvalidDataException("Registry target has an invalid value kind.");

            return new RegistryTarget
            {
                SubKey = subKey,
                ValueName = valueName,
                DeclaredKind = parsedKind,
            };
        }

        private static string GetString(Dictionary<string, object> values, string name, bool allowEmpty = false)
        {
            object value;
            if (values == null || !values.TryGetValue(name, out value) || !(value is string))
                throw new InvalidDataException("Registry target is missing '" + name + "'.");

            var text = (string)value;
            if (!allowEmpty && string.IsNullOrWhiteSpace(text))
                throw new InvalidDataException("Registry target field '" + name + "' is empty.");
            return text;
        }

        private static object EncodeValue(RegistryValueKind kind, object value)
        {
            if (kind == RegistryValueKind.Binary || kind == RegistryValueKind.None)
            {
                var bytes = value as byte[];
                if (bytes == null) throw new InvalidDataException("Binary registry value is not byte data.");
                return Convert.ToBase64String(bytes);
            }
            return value;
        }

        private static object DecodeValue(RegistryValueKind kind, object encoded)
        {
            switch (kind)
            {
                case RegistryValueKind.String:
                case RegistryValueKind.ExpandString:
                    return Convert.ToString(encoded);
                case RegistryValueKind.DWord:
                    return Convert.ToInt32(encoded);
                case RegistryValueKind.QWord:
                    return Convert.ToInt64(encoded);
                case RegistryValueKind.MultiString:
                    if (encoded is string[]) return (string[])encoded;
                    if (encoded is object[] objects)
                    {
                        var result = new string[objects.Length];
                        for (var i = 0; i < objects.Length; i++) result[i] = Convert.ToString(objects[i]);
                        return result;
                    }
                    throw new InvalidDataException("MultiString registry restore data is invalid.");
                case RegistryValueKind.Binary:
                case RegistryValueKind.None:
                    try
                    {
                        return Convert.FromBase64String(Convert.ToString(encoded));
                    }
                    catch (FormatException ex)
                    {
                        throw new InvalidDataException("Binary registry restore data is invalid.", ex);
                    }
                default:
                    throw new InvalidDataException("Unsupported registry value kind: " + kind);
            }
        }

        private sealed class RegistryTarget
        {
            public string SubKey { get; set; }
            public string ValueName { get; set; }
            public RegistryValueKind DeclaredKind { get; set; }
        }
    }
}
