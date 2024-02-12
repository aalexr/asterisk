"""add transport attribute to identify

Revision ID: 5c9dfe2b851c
Revises: 24c12d8e9014
Create Date: 2024-02-14 18:35:38.917103

"""

# revision identifiers, used by Alembic.
revision = '5c9dfe2b851c'
down_revision = '24c12d8e9014'

from alembic import op
import sqlalchemy as sa


def upgrade():
    op.add_column('ps_endpoint_id_ips', sa.Column('transport', sa.String(8)))

def downgrade():
    op.drop_column('ps_endpoint_id_ips', 'transport')
