namespace System
{
	static partial class PlatformDetection
	{
		public static bool IsOSX => true;
		public static Version OpenSslVersion => new Version (-1, 0);
		public static bool IsDebian => false;
		public static bool IsMacOsHighSierraOrHigher => true;
		public static int WindowsVersion => -1;

		public static bool SupportsX509Chain => UsingBtls;
		public static bool SupportsCertRevocation => !UsingBtls;
		public static bool UsingBtls => string.Equals (Environment.GetEnvironmentVariable ("MONO_TLS_PROVIDER"), "btls");
	}
}
