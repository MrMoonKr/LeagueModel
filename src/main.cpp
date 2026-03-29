#include "app/league_model_app.hpp"

int main(int argc, char** argv)
{
	LeagueModel::LeagueModelApp app(argc, argv);
	if (!app.InitInstance())
		return -1;

	const int exitCode = app.Run();
	app.ExitInstance();
	return exitCode;
}
