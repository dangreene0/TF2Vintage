using Microsoft.Build.Construction;
using Microsoft.Build.Evaluation;

string Version = "";
string Solution = "";

foreach (var arg in args)
{
	if(arg.StartsWith('/'))
	{
		string argument = arg.Substring(1);
		switch (argument)
		{
			case "2013":
				Version = "v120_xp";
				break;
			case "2015":
				Version = "v140_xp";
				break;
			case "2017":
				Version = "v141";
				break;
			case "2019":
				Version = "v142";
				break;
			case "2022":
				Version = "v143";
				break;
			default:
				Solution = arg.Substring(1);
				break;
		}
	}
	else
	{
		Solution = arg;
	}
}

if (Solution.Equals(""))
	return;

var solutionFile = SolutionFile.Parse(Path.Combine(Directory.GetCurrentDirectory(), Solution));
foreach(var project in solutionFile.ProjectsInOrder)
{
	var projectFile = File.OpenText(project.AbsolutePath);
	var tmpFile = File.Create(string.Concat(project.AbsolutePath, ".tmp"));
	while (!projectFile.EndOfStream)
	{
		var line = projectFile.ReadLine();
		if (line != null)
		{
			if (line.Contains("PlatformToolset"))
			{
				int startIndex = line.IndexOf('>') + 1;
				int endIndex = line.IndexOf('<', startIndex);
				var toolset = line.Substring(startIndex, endIndex - startIndex);
				if (line.Contains(toolset))
					line = line.Replace(toolset, Version);
			}

			foreach (var c in line.ToArray())
			{
				tmpFile.WriteByte((byte)c);
			}
			tmpFile.WriteByte((byte)'\n');
		}
	}

	tmpFile.Close();
	projectFile.Close();

	File.Replace(tmpFile.Name, project.AbsolutePath, string.Concat(project.AbsolutePath, ".bak"));
}