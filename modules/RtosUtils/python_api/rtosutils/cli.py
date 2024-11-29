import sys
import atexit
import logging
import click

from rich.console import Console

# ProtoRpc modules
from protorpc.cli import get_params
from protorpc.cli.common_opts import cli_common_opts, cli_init
from protorpc.cli.common_opts import CONTEXT_SETTINGS

# Callset classes
from rtosutils import RtosUtils


logger = logging.getLogger(__name__)

connections = []


def on_exit():
    """Cleanup actions on program exit.
    """
    logger.info("Closing connections on exit.")
    for con in connections:
        con.close()


@click.group(context_settings=CONTEXT_SETTINGS)
@cli_common_opts
@click.pass_context
def cli(ctx, **kwargs):
    """CLI application for calling RtosUtils RPCs.
    """
    global connections

    params = get_params(**kwargs)

    try:
        api, conn = cli_init(ctx, params)
    except Exception as e:
        logger.error(f"Exiting due to error: {str(e)}")
        sys.exit(1)

    ctx.obj['rtos_utils'] = RtosUtils(api)
    ctx.obj['conn'] = conn

    connections.append(conn)
    atexit.register(on_exit)


@cli.command
@click.pass_context
def get_tasks(ctx, **kwargs):
    """Prints a table of RTOS tasks from device.
    """
    params = get_params(**kwargs)
    cli_params = ctx.obj['cli_params']

    rtos_utils = ctx.obj['rtos_utils']
    tbl = rtos_utils.get_system_tasks_table()
    con = Console()
    con.print(tbl)


def main():
    cli(obj={})


if __name__ == "__main__":
    main()
