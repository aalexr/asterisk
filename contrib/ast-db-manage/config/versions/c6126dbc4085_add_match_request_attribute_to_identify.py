"""add match_request attribute to identify

Revision ID: c6126dbc4085
Revises: 24c12d8e9014
Create Date: 2024-02-14 20:59:29.628318

"""

# revision identifiers, used by Alembic.
revision = 'c6126dbc4085'
down_revision = '24c12d8e9014'

from alembic import op
import sqlalchemy as sa


def upgrade():
    op.add_column('ps_endpoint_id_ips', sa.Column('match_request_uri', sa.String(255)))

def downgrade():
    op.drop_column('ps_endpoint_id_ips', 'match_request_uri')
