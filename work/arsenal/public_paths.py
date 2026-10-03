"""Public helpers target an explicitly selected, licensed local project."""
import os
from pathlib import Path
WORKSPACE=Path(__file__).resolve().parents[2]
PROJECT=Path(os.environ.get('BORDERTOWN_PROJECT',str(WORKSPACE))).resolve()
