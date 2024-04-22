/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Linq;
using System.Reflection;
using System.Runtime.Serialization;

namespace UnrealFrontend
{
	/// <summary>
	/// Class from which simple app command-line parsers can be derived. All that the derived class needs to define are
	/// public string and/or boolean properties. The command line format is:
	/// e.g.	/stringpropertyname=stringvalue
	///			/propertyname="string value with whitespace"
	///			-stringpropertyname=stringvalue
	///			-stringpropertyname="string value with whitespace"
	///			/booleanpropertyname
	///			-booleanpropertyname
	/// </summary>
	public class CommandLine
	{
		private static readonly char[] QuoteChars = new[] { '\"', '\'' };
		private static readonly char[] EqualsChars = new[] { '=' };
		private static readonly char[] ParamChars = new[] { '/', '-' };

		/// <summary>
		/// Parse a string containing command line args and sets matching public properties.
		/// </summary>
		/// <param name="Args">A string containing command line args (see class summary desc above).</param>
		public void Parse(string[] Args)
		{
			Type CommandLineType = GetType();
			PropertyInfo[] Props = CommandLineType.GetProperties();

			foreach (PropertyInfo Prop in Props)
			{
				if (typeof(string) == Prop.PropertyType)
				{
					Prop.SetValue(this, string.Empty, null);
				}
				if (typeof(bool) == Prop.PropertyType)
				{
					Prop.SetValue(this, false, null);
				}
			}

			for (int ArgIdx = 0; ArgIdx < Args.Length; ArgIdx++)
			{
				string Arg = Args[ArgIdx];
				if (!string.IsNullOrEmpty(Arg))
				{
					// All args must begin with one of ParamChars
					if (!ParamChars.Contains(Arg[0]))
					{
						string ParamCharsString = string.Join(", ", (from C in ParamChars select string.Format("'{0}'", C)).ToArray());
						throw new CommandLineFormatException(this, Args, "All arguments must begin with one of these characters " + ParamCharsString);
					}

					string[] Tokens = Arg.TrimStart(ParamChars).Split(EqualsChars, 2);

					if (1 == Tokens.Length)
					{
						// boolean property/flag
						if (string.IsNullOrEmpty(Tokens[0]))
						{
							throw new CommandLineFormatException(this, Args, "Command line syntax error.");
						}

						string PropertyName = Tokens[0].Substring(0, 1).ToUpperInvariant() +
							                    Tokens[0].Substring(1).ToLowerInvariant();

						try
						{
							PropertyInfo Prop =
								Props.Single(P => 0 == string.Compare(P.Name, PropertyName, StringComparison.InvariantCultureIgnoreCase));

							if (typeof (bool) != Prop.PropertyType)
							{
								throw new CommandLineFormatException(this, Args,
									                                    string.Format("Boolean-type argument {0} should be of type {1}", Arg,
									                                                Prop.PropertyType.Name));
							}

							Prop.SetValue(this, true, null);
						}
						catch (InvalidOperationException e)
						{
							throw new CommandLineUnknownArgumentException(this, Arg, "Invalid argument " + Arg, e);
						}
					}
					else if (2 == Tokens.Length)
					{
						// string property with value
						if (string.IsNullOrEmpty(Tokens[0]) || string.IsNullOrEmpty(Tokens[1]))
						{
							throw new CommandLineFormatException(this, Args, "Command line syntax error.");
						}

						string PropertyName = Tokens[0].Substring(0, 1).ToUpperInvariant() +
												Tokens[0].Substring(1).ToLowerInvariant();
						string Value = Tokens[1].Trim(QuoteChars);

						try
						{
							PropertyInfo Prop =
								Props.Single(P => 0 == string.Compare(P.Name, PropertyName, StringComparison.InvariantCultureIgnoreCase));

							if (typeof(string) != Prop.PropertyType)
							{
								throw new CommandLineFormatException(this, Args,
																		string.Format("String-type argument {0} should be of type {1}", Arg,
																					Prop.PropertyType.Name));
							}

							Prop.SetValue(this, Value, null);
						}
						catch (InvalidOperationException e)
						{
							throw new CommandLineUnknownArgumentException(this, Arg, "Invalid argument " + Arg, e);
						}
					}
				}
			}
		}
	}

	/// <summary>
	/// Exception raised by CommandLine when a command line is not of the expected format.
	/// </summary>
	[Serializable]
	public class CommandLineFormatException : ApplicationException
	{
		public CommandLine CommandLineObject { get; set; }
		public string[] CommandLineArgs { get; set; }

		public CommandLineFormatException(CommandLine ObjectParam, string[] ArgsParam, string message) 
			: base(message)
		{
			CommandLineObject = ObjectParam;
			CommandLineArgs = ArgsParam;
		}

		public CommandLineFormatException(CommandLine ObjectParam, string[] ArgsParam, string message, Exception innerException)
			: base(message, innerException)
		{
			CommandLineObject = ObjectParam;
			CommandLineArgs = ArgsParam;
		}

		protected CommandLineFormatException(SerializationInfo info, StreamingContext context)
			: base(info, context)
		{
		}
	}

	/// <summary>
	/// Exception raised by CommandLine when a command line contains an unrecognised argument.
	/// </summary>
	[Serializable]
	public class CommandLineUnknownArgumentException : ApplicationException
	{
		public CommandLine CommandLineObject { get; set; }
		public string CommandLineUnknownArgument { get; set; }

		public CommandLineUnknownArgumentException(CommandLine ObjectParam, string UnknownArgumentParam, string message)
			: base(message)
		{
			CommandLineObject = ObjectParam;
			CommandLineUnknownArgument = UnknownArgumentParam;
		}

		public CommandLineUnknownArgumentException(CommandLine ObjectParam, string UnknownArgumentParam, string message, Exception innerException)
			: base(message, innerException)
		{
			CommandLineObject = ObjectParam;
			CommandLineUnknownArgument = UnknownArgumentParam;
		}

		protected CommandLineUnknownArgumentException(SerializationInfo info, StreamingContext context)
			: base(info, context)
		{
		}
	}

	/// <summary>
	/// Derived CommandLine used for parsing the UnrealFrontend command line.
	/// </summary>
	public class FrontendCommandLine : CommandLine
	{
		/// <summary>
		/// Profile property. If set to a profile xml file in the profiles folder will load that profile when the app starts.
		/// </summary>
		public string Profile { get; set; }
		/// <summary>
		/// Autostart property. If in the command line, makes the current profile auto-start running when the app starts. 
		/// </summary>
		public bool Autostart { get; set; }
		/// <summary>
		/// Autoquit property. If in the command line and Autostart property is also true, makes the app close on successful complation of the run profile job.
		/// </summary>
		public bool Autoquit { get; set; }
	}
}
