"""Add TCP keepalive settings to ps_transports

Revision ID: 8fce8496f03e
Revises: 74dc751dfe8e
Create Date: 2024-03-18 17:00:17.148018

"""

# revision identifiers, used by Alembic.
revision = '8fce8496f03e'
down_revision = '74dc751dfe8e'

from alembic import op
import sqlalchemy as sa

def upgrade():
    op.add_column('ps_transports', sa.Column('tcp_keepalive_enable', sa.Boolean(), nullable=True))
    op.add_column('ps_transports', sa.Column('tcp_keepalive_idle_time', sa.Integer(), nullable=True))
    op.add_column('ps_transports', sa.Column('tcp_keepalive_interval_time', sa.Integer(), nullable=True))
    op.add_column('ps_transports', sa.Column('tcp_keepalive_probe_count', sa.Integer(), nullable=True))

def downgrade():
    op.drop_column('ps_transports', 'tcp_keepalive_enable')
    op.drop_column('ps_transports', 'tcp_keepalive_idle_time')
    op.drop_column('ps_transports', 'tcp_keepalive_interval_time')
    op.drop_column('ps_transports', 'tcp_keepalive_probe_count')
