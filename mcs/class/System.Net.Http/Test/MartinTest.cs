using System;
using System.Collections;
using System.Collections.Generic;
using NUnit.Framework;

using MonoTests.Helpers;

[TestFixture]
class MartinTest
{
    [Test]
    public void TestEnvironment ()
    {
        Console.Error.WriteLine ("MARTIN TEST!");
	var vars = Environment.GetEnvironmentVariables ();
	foreach (DictionaryEntry env in vars)
		Console.Error.WriteLine ($"  ENV: {env.Key} = {env.Value}");
	
	var uriEnv = vars ["MONO_URI_DOTNETRELATIVEORABSOLUTE"];
	Assert.AreEqual (uriEnv, "true");
    }
}