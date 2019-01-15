using System;
using System.Collections;
using System.Collections.Generic;
using MonoTests.Helpers;
using NUnit.Framework;

namespace MonoTests.System.Net.Http
{
	[TestFixture]
	class MartinTest
	{
		[Test]
		public void TestEnvironment ()
		{
			var vars = Environment.GetEnvironmentVariables ();
//			foreach (DictionaryEntry env in vars)
//				Console.Error.WriteLine ($"  ENV: {env.Key} = {env.Value}");

			var uriEnv = vars["MONO_URI_DOTNETRELATIVEORABSOLUTE"];
			Console.Error.WriteLine ($"MARTIN TEST: envvar={uriEnv != null} socketshandler={HttpClientTestHelpers.UsingSocketsHandler}");
			Assert.AreEqual (null, uriEnv, "#1");
		}
	}
}